#include <catch2/catch_test_macros.hpp>
#include <pulp/render/draco_decoder.hpp>
#include <pulp/render/draco_scene_adapter.hpp>
#include <pulp/render/gpu_surface.hpp>
#include <pulp/render/renderer3d.hpp>
#include <pulp/scene/bake_preflight.hpp>
#include <pulp/scene/gltf_loader.hpp>
#include <pulp/scene/scene_data.hpp>
#include <pulp/scene/sidecar.hpp>

#ifdef PULP_TEST_HAS_DRACO
#include <draco/compression/encode.h>
#include <draco/core/encoder_buffer.h>
#include <draco/core/vector_d.h>
#include <draco/mesh/triangle_soup_mesh_builder.h>
#endif

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace pulp::render;

namespace {

inline constexpr uint64_t kMacMetalHardcodedCubeFingerprint =
    15925498132503966243ULL;
inline constexpr uint64_t kMacMetalBoxTexturedFixtureFingerprint =
    5845745157752120258ULL;

bool is_mac_metal_adapter(const HardcodedCubeRenderResult& result) {
    return result.adapter_info_available &&
           result.adapter_backend_type == "Metal";
}

bool has_native_runtime_gap(const pulp::scene::BakePreflightReport& report,
                            const std::string& feature) {
    for (const auto& gap : report.native_runtime_gaps) {
        if (gap.feature == feature) {
            return true;
        }
    }
    return false;
}

void append_u32(std::vector<uint8_t>& bytes, uint32_t value) {
    bytes.push_back(static_cast<uint8_t>(value & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 16u) & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 24u) & 0xffu));
}

void append_f32(std::vector<uint8_t>& bytes, float value) {
    uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(raw));
    append_u32(bytes, raw);
}

void append_u16(std::vector<uint8_t>& bytes, uint16_t value) {
    bytes.push_back(static_cast<uint8_t>(value & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
}

void pad4(std::vector<uint8_t>& bytes, uint8_t fill = 0) {
    while (bytes.size() % 4u != 0u) {
        bytes.push_back(fill);
    }
}

struct ForegroundRegion {
    uint32_t pixel_count = 0;
    uint32_t min_x = std::numeric_limits<uint32_t>::max();
    uint32_t min_y = std::numeric_limits<uint32_t>::max();
    uint32_t max_x = 0;
    uint32_t max_y = 0;
};

ForegroundRegion find_foreground_region(const std::vector<uint8_t>& rgba,
                                        uint32_t width,
                                        uint32_t height) {
    ForegroundRegion region;
    if (rgba.size() < 4 || width == 0 || height == 0) {
        return region;
    }

    const uint8_t bg_r = rgba[0];
    const uint8_t bg_g = rgba[1];
    const uint8_t bg_b = rgba[2];
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t offset = (static_cast<size_t>(y) * width + x) * 4u;
            const int dr = static_cast<int>(rgba[offset + 0]) - static_cast<int>(bg_r);
            const int dg = static_cast<int>(rgba[offset + 1]) - static_cast<int>(bg_g);
            const int db = static_cast<int>(rgba[offset + 2]) - static_cast<int>(bg_b);
            const int delta = (dr < 0 ? -dr : dr) +
                              (dg < 0 ? -dg : dg) +
                              (db < 0 ? -db : db);
            if (delta <= 18) {
                continue;
            }

            ++region.pixel_count;
            region.min_x = std::min(region.min_x, x);
            region.min_y = std::min(region.min_y, y);
            region.max_x = std::max(region.max_x, x);
            region.max_y = std::max(region.max_y, y);
        }
    }
    return region;
}

uint32_t foreground_width(const ForegroundRegion& region) {
    if (region.pixel_count == 0 || region.max_x < region.min_x) {
        return 0;
    }
    return region.max_x - region.min_x + 1u;
}

double foreground_center_x(const ForegroundRegion& region) {
    if (region.pixel_count == 0 || region.max_x < region.min_x) {
        return 0.0;
    }
    return (static_cast<double>(region.min_x) +
            static_cast<double>(region.max_x)) * 0.5;
}

double average_foreground_luma(const std::vector<uint8_t>& rgba,
                               uint32_t width,
                               uint32_t height) {
    if (rgba.size() < static_cast<size_t>(width) * height * 4u ||
        width == 0 || height == 0) {
        return 0.0;
    }

    const uint8_t bg_r = rgba[0];
    const uint8_t bg_g = rgba[1];
    const uint8_t bg_b = rgba[2];
    double sum = 0.0;
    uint32_t count = 0;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t offset = (static_cast<size_t>(y) * width + x) * 4u;
            const int dr = static_cast<int>(rgba[offset + 0]) - static_cast<int>(bg_r);
            const int dg = static_cast<int>(rgba[offset + 1]) - static_cast<int>(bg_g);
            const int db = static_cast<int>(rgba[offset + 2]) - static_cast<int>(bg_b);
            const int delta = (dr < 0 ? -dr : dr) +
                              (dg < 0 ? -dg : dg) +
                              (db < 0 ? -db : db);
            if (delta <= 18) {
                continue;
            }
            sum += 0.2126 * static_cast<double>(rgba[offset + 0]) +
                   0.7152 * static_cast<double>(rgba[offset + 1]) +
                   0.0722 * static_cast<double>(rgba[offset + 2]);
            ++count;
        }
    }
    return count == 0 ? 0.0 : sum / static_cast<double>(count);
}

double average_image_luma(const std::vector<uint8_t>& rgba,
                          uint32_t width,
                          uint32_t height) {
    if (rgba.size() < static_cast<size_t>(width) * height * 4u ||
        width == 0 || height == 0) {
        return 0.0;
    }

    double sum = 0.0;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t offset = (static_cast<size_t>(y) * width + x) * 4u;
            sum += 0.2126 * static_cast<double>(rgba[offset + 0]) +
                   0.7152 * static_cast<double>(rgba[offset + 1]) +
                   0.0722 * static_cast<double>(rgba[offset + 2]);
        }
    }
    return sum / static_cast<double>(width * height);
}

double average_foreground_channel(const std::vector<uint8_t>& rgba,
                                  uint32_t width,
                                  uint32_t height,
                                  uint32_t channel) {
    if (rgba.size() < static_cast<size_t>(width) * height * 4u ||
        width == 0 || height == 0 || channel > 2) {
        return 0.0;
    }

    const uint8_t bg_r = rgba[0];
    const uint8_t bg_g = rgba[1];
    const uint8_t bg_b = rgba[2];
    double sum = 0.0;
    uint32_t count = 0;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t offset = (static_cast<size_t>(y) * width + x) * 4u;
            const int dr = static_cast<int>(rgba[offset + 0]) - static_cast<int>(bg_r);
            const int dg = static_cast<int>(rgba[offset + 1]) - static_cast<int>(bg_g);
            const int db = static_cast<int>(rgba[offset + 2]) - static_cast<int>(bg_b);
            const int delta = (dr < 0 ? -dr : dr) +
                              (dg < 0 ? -dg : dg) +
                              (db < 0 ? -db : db);
            if (delta <= 18) {
                continue;
            }
            sum += static_cast<double>(rgba[offset + channel]);
            ++count;
        }
    }
    return count == 0 ? 0.0 : sum / static_cast<double>(count);
}

std::vector<uint8_t> make_alpha_checker_png();

std::vector<uint8_t> make_renderable_textured_glb() {
    std::vector<uint8_t> bin;
    for (float value : {
             -1.0f, -1.0f, 0.0f,
              1.0f, -1.0f, 0.0f,
              0.0f,  1.0f, 0.0f,
         }) {
        append_f32(bin, value);
    }
    for (float value : {
             0.0f, 0.0f,
             1.0f, 0.0f,
             0.5f, 1.0f,
         }) {
        append_f32(bin, value);
    }
    append_u16(bin, 0);
    append_u16(bin, 1);
    append_u16(bin, 2);
    pad4(bin);

    const std::vector<uint8_t> checker_png = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xb6, 0x0d, 0x24, 0x00, 0x00, 0x00,
        0x11, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0x0f, 0x01, 0x0c,
        0x0c, 0x20, 0x02, 0xc4, 0x02, 0x00, 0x80, 0x91, 0x0d, 0xf3, 0x7e, 0xc1,
        0xff, 0xde, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
        0x60, 0x82,
    };
    const uint32_t image_offset = static_cast<uint32_t>(bin.size());
    bin.insert(bin.end(), checker_png.begin(), checker_png.end());
    const uint32_t image_length = static_cast<uint32_t>(checker_png.size());
    pad4(bin);

    const std::string json =
        R"({"asset":{"version":"2.0","generator":"pulp-test-renderer3d"},"buffers":[{"byteLength":)" +
        std::to_string(bin.size()) +
        R"(}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":36,"byteLength":24,"target":34962},{"buffer":0,"byteOffset":60,"byteLength":6,"target":34963},{"buffer":0,"byteOffset":)" +
        std::to_string(image_offset) +
        R"(,"byteLength":)" +
        std::to_string(image_length) +
        R"(}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-1,-1,0],"max":[1,1,0]},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],"images":[{"bufferView":3,"mimeType":"image/png","name":"checker"}],"samplers":[{"name":"linearClamp","magFilter":9729,"minFilter":9729,"wrapS":33071,"wrapT":33071}],"textures":[{"source":0,"sampler":0,"name":"checkerTexture"}],"materials":[{"name":"mat","pbrMetallicRoughness":{"baseColorFactor":[0.9,0.35,0.15,1],"baseColorTexture":{"index":0}},"doubleSided":true}],"meshes":[{"name":"triangle","primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0,"mode":4}]}],"nodes":[{"name":"root","mesh":0,"translation":[1,2,3]}],"scenes":[{"nodes":[0]}],"scene":0})";

    std::vector<uint8_t> json_bytes(json.begin(), json.end());
    pad4(json_bytes, 0x20);

    std::vector<uint8_t> glb;
    append_u32(glb, 0x46546c67u);
    append_u32(glb, 2u);
    append_u32(glb, static_cast<uint32_t>(12u + 8u + json_bytes.size() + 8u + bin.size()));
    append_u32(glb, static_cast<uint32_t>(json_bytes.size()));
    append_u32(glb, 0x4e4f534au);
    glb.insert(glb.end(), json_bytes.begin(), json_bytes.end());
    append_u32(glb, static_cast<uint32_t>(bin.size()));
    append_u32(glb, 0x004e4942u);
    glb.insert(glb.end(), bin.begin(), bin.end());
    return glb;
}

std::vector<uint8_t> make_multi_node_textured_glb() {
    std::vector<uint8_t> bin;
    for (float value : {
             -0.65f, -0.65f, 0.0f,
              0.65f, -0.65f, 0.0f,
              0.0f,   0.65f, 0.0f,
         }) {
        append_f32(bin, value);
    }
    for (float value : {
             0.0f, 0.0f,
             1.0f, 0.0f,
             0.5f, 1.0f,
         }) {
        append_f32(bin, value);
    }
    append_u16(bin, 0);
    append_u16(bin, 1);
    append_u16(bin, 2);
    pad4(bin);

    const std::vector<uint8_t> checker_png = make_alpha_checker_png();
    const uint32_t image_offset = static_cast<uint32_t>(bin.size());
    bin.insert(bin.end(), checker_png.begin(), checker_png.end());
    const uint32_t image_length = static_cast<uint32_t>(checker_png.size());
    pad4(bin);

    const std::string json =
        R"({"asset":{"version":"2.0","generator":"pulp-test-renderer3d-multi-node"},"buffers":[{"byteLength":)" +
        std::to_string(bin.size()) +
        R"(}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":36,"byteLength":24,"target":34962},{"buffer":0,"byteOffset":60,"byteLength":6,"target":34963},{"buffer":0,"byteOffset":)" +
        std::to_string(image_offset) +
        R"(,"byteLength":)" +
        std::to_string(image_length) +
        R"(}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-0.65,-0.65,0],"max":[0.65,0.65,0]},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],"images":[{"bufferView":3,"mimeType":"image/png","name":"checker"}],"samplers":[{"name":"linearClamp","magFilter":9729,"minFilter":9729,"wrapS":33071,"wrapT":33071}],"textures":[{"source":0,"sampler":0,"name":"checkerTexture"}],"materials":[{"name":"multi-node-material","pbrMetallicRoughness":{"baseColorFactor":[1,1,1,1],"baseColorTexture":{"index":0}},"doubleSided":true}],"meshes":[{"name":"shared-triangle","primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0,"mode":4}]}],"nodes":[{"name":"left","mesh":0,"translation":[-1.05,0,0]},{"name":"right","mesh":0,"translation":[1.05,0,0]}],"scenes":[{"nodes":[0,1]}],"scene":0})";

    std::vector<uint8_t> json_bytes(json.begin(), json.end());
    pad4(json_bytes, 0x20);

    std::vector<uint8_t> glb;
    append_u32(glb, 0x46546c67u);
    append_u32(glb, 2u);
    append_u32(glb, static_cast<uint32_t>(12u + 8u + json_bytes.size() + 8u + bin.size()));
    append_u32(glb, static_cast<uint32_t>(json_bytes.size()));
    append_u32(glb, 0x4e4f534au);
    glb.insert(glb.end(), json_bytes.begin(), json_bytes.end());
    append_u32(glb, static_cast<uint32_t>(bin.size()));
    append_u32(glb, 0x004e4942u);
    glb.insert(glb.end(), bin.begin(), bin.end());
    return glb;
}

std::vector<uint8_t> make_renderable_textured_cube_glb() {
    struct Vertex {
        float x, y, z;
        float nx, ny, nz;
        float u, v;
    };

    const std::array<Vertex, 24> vertices = {{
        {-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f},
        { 0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f},
        { 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f},
        {-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f},
        { 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f},
        {-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f},
        {-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f},
        { 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f},
        {-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f},
        {-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f},
        {-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f},
        {-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f},
        { 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f},
        { 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f},
        { 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f},
        { 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f},
        {-0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f},
        { 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f},
        { 0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f},
        {-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f},
        {-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f},
        { 0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f},
        { 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f},
        {-0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f},
    }};
    const std::array<uint16_t, 36> indices = {{
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23,
    }};

    std::vector<uint8_t> bin;
    const uint32_t positions_offset = static_cast<uint32_t>(bin.size());
    for (const auto& vertex : vertices) {
        append_f32(bin, vertex.x);
        append_f32(bin, vertex.y);
        append_f32(bin, vertex.z);
    }
    const uint32_t positions_length =
        static_cast<uint32_t>(bin.size()) - positions_offset;

    const uint32_t normals_offset = static_cast<uint32_t>(bin.size());
    for (const auto& vertex : vertices) {
        append_f32(bin, vertex.nx);
        append_f32(bin, vertex.ny);
        append_f32(bin, vertex.nz);
    }
    const uint32_t normals_length =
        static_cast<uint32_t>(bin.size()) - normals_offset;

    const uint32_t texcoords_offset = static_cast<uint32_t>(bin.size());
    for (const auto& vertex : vertices) {
        append_f32(bin, vertex.u);
        append_f32(bin, vertex.v);
    }
    const uint32_t texcoords_length =
        static_cast<uint32_t>(bin.size()) - texcoords_offset;

    const uint32_t indices_offset = static_cast<uint32_t>(bin.size());
    for (const auto index : indices) {
        append_u16(bin, index);
    }
    const uint32_t indices_length =
        static_cast<uint32_t>(bin.size()) - indices_offset;
    pad4(bin);

    const std::vector<uint8_t> checker_png = make_alpha_checker_png();
    const uint32_t image_offset = static_cast<uint32_t>(bin.size());
    bin.insert(bin.end(), checker_png.begin(), checker_png.end());
    const uint32_t image_length = static_cast<uint32_t>(checker_png.size());
    pad4(bin);

    const std::string json =
        R"({"asset":{"version":"2.0","generator":"pulp-test-renderer3d-box-textured"},"buffers":[{"byteLength":)" +
        std::to_string(bin.size()) +
        R"(}],"bufferViews":[{"buffer":0,"byteOffset":)" +
        std::to_string(positions_offset) +
        R"(,"byteLength":)" +
        std::to_string(positions_length) +
        R"(,"target":34962},{"buffer":0,"byteOffset":)" +
        std::to_string(normals_offset) +
        R"(,"byteLength":)" +
        std::to_string(normals_length) +
        R"(,"target":34962},{"buffer":0,"byteOffset":)" +
        std::to_string(texcoords_offset) +
        R"(,"byteLength":)" +
        std::to_string(texcoords_length) +
        R"(,"target":34962},{"buffer":0,"byteOffset":)" +
        std::to_string(indices_offset) +
        R"(,"byteLength":)" +
        std::to_string(indices_length) +
        R"(,"target":34963},{"buffer":0,"byteOffset":)" +
        std::to_string(image_offset) +
        R"(,"byteLength":)" +
        std::to_string(image_length) +
        R"(}],"accessors":[{"bufferView":0,"componentType":5126,"count":24,"type":"VEC3","min":[-0.5,-0.5,-0.5],"max":[0.5,0.5,0.5]},{"bufferView":1,"componentType":5126,"count":24,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":24,"type":"VEC2"},{"bufferView":3,"componentType":5123,"count":36,"type":"SCALAR"}],"images":[{"bufferView":4,"mimeType":"image/png","name":"checker"}],"samplers":[{"name":"linearClamp","magFilter":9729,"minFilter":9729,"wrapS":33071,"wrapT":33071}],"textures":[{"source":0,"sampler":0,"name":"checkerTexture"}],"materials":[{"name":"box-textured-material","pbrMetallicRoughness":{"baseColorFactor":[1,1,1,1],"baseColorTexture":{"index":0}},"doubleSided":false}],"meshes":[{"name":"BoxTexturedLike","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0,"mode":4}]}],"nodes":[{"name":"BoxTexturedLike","mesh":0,"rotation":[0.12,0.28,0,0.952]}],"scenes":[{"nodes":[0]}],"scene":0})";

    std::vector<uint8_t> json_bytes(json.begin(), json.end());
    pad4(json_bytes, 0x20);

    std::vector<uint8_t> glb;
    append_u32(glb, 0x46546c67u);
    append_u32(glb, 2u);
    append_u32(glb, static_cast<uint32_t>(12u + 8u + json_bytes.size() + 8u + bin.size()));
    append_u32(glb, static_cast<uint32_t>(json_bytes.size()));
    append_u32(glb, 0x4e4f534au);
    glb.insert(glb.end(), json_bytes.begin(), json_bytes.end());
    append_u32(glb, static_cast<uint32_t>(bin.size()));
    append_u32(glb, 0x004e4942u);
    glb.insert(glb.end(), bin.begin(), bin.end());
    return glb;
}

std::filesystem::path write_temp_file(const std::string& name,
                                      const std::vector<uint8_t>& bytes) {
    auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    REQUIRE(file.good());
    return path;
}

void write_file(const std::filesystem::path& path,
                const std::vector<uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    REQUIRE(file.good());
}

void write_file(const std::filesystem::path& path,
                const std::string& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    REQUIRE(file.good());
}

std::filesystem::path scene3d_fixture_path(const std::string& relative) {
#ifdef PULP_TEST_SCENE3D_FIXTURE_DIR
    return std::filesystem::path(PULP_TEST_SCENE3D_FIXTURE_DIR) / relative;
#else
    return std::filesystem::path("test/fixtures/scene3d") / relative;
#endif
}

pulp::scene::SceneData make_child_node_render_scene() {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = "child-material";
    material.base_color_factor[0] = 0.25f;
    material.base_color_factor[1] = 0.75f;
    material.base_color_factor[2] = 0.35f;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "child-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData root;
    root.name = "root";
    root.translation[0] = 3.0f;
    root.children.push_back(1);
    scene.nodes.push_back(std::move(root));

    pulp::scene::NodeData child;
    child.name = "child";
    child.mesh = 0;
    child.translation[1] = -2.0f;
    child.scale[0] = 0.75f;
    child.scale[1] = 1.25f;
    scene.nodes.push_back(std::move(child));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_transformed_uv_render_scene() {
    pulp::scene::SceneData scene;

    pulp::scene::TextureData texture;
    texture.name = "fallback-base";
    texture.mime_type = "image/png";
    texture.encoded_bytes = {0x89, 0x50, 0x4e, 0x47};
    scene.textures.push_back(std::move(texture));

    pulp::scene::MaterialData material;
    material.name = "transformed-uv-material";
    material.base_color_texture = 0;
    material.base_color_texcoord = 0;
    material.base_color_transform.enabled = true;
    material.base_color_transform.offset[0] = 0.125f;
    material.base_color_transform.offset[1] = 0.25f;
    material.base_color_transform.scale[0] = 1.5f;
    material.base_color_transform.scale[1] = 0.5f;
    material.base_color_transform.rotation = 0.2f;
    material.base_color_transform.texcoord_override = 1;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.texcoord1 = {
        0.25f, 0.25f,
        0.75f, 0.25f,
        0.5f, 0.75f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "transformed-uv-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_mipmap_sampler_render_scene() {
    pulp::scene::SceneData scene;

    pulp::scene::TextureData texture;
    texture.name = "fallback-with-mipmap-sampler";
    texture.mime_type = "image/png";
    texture.encoded_bytes = {0x89, 0x50, 0x4e, 0x47};
    scene.textures.push_back(std::move(texture));

    pulp::scene::TextureSamplerData sampler;
    sampler.name = "mipmap-linear";
    sampler.mag_filter = pulp::scene::TextureSamplerData::Filter::linear;
    sampler.min_filter =
        pulp::scene::TextureSamplerData::Filter::linear_mipmap_linear;
    scene.texture_samplers.push_back(sampler);

    pulp::scene::MaterialData material;
    material.name = "mipmap-sampler-material";
    material.base_color_texture = 0;
    material.base_color_sampler = 0;
    material.base_color_factor[0] = 0.8f;
    material.base_color_factor[1] = 0.8f;
    material.base_color_factor[2] = 0.8f;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "mipmap-sampler-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_multi_primitive_render_scene() {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = "multi-primitive-material";
    material.base_color_factor[0] = 0.85f;
    material.base_color_factor[1] = 0.45f;
    material.base_color_factor[2] = 0.20f;
    material.double_sided = true;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData left;
    left.positions = {
        -2.0f, -1.0f, 0.0f,
        -0.4f, -1.0f, 0.0f,
        -1.2f,  1.0f, 0.0f,
    };
    left.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    left.indices = {0, 1, 2};
    left.material = 0;

    pulp::scene::PrimitiveData right;
    right.positions = {
         0.4f, -1.0f, 0.0f,
         2.0f, -1.0f, 0.0f,
         1.2f,  1.0f, 0.0f,
    };
    right.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    right.indices = {0, 1, 2};
    right.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "two-triangle-mesh";
    mesh.primitives.push_back(std::move(left));
    mesh.primitives.push_back(std::move(right));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_mixed_material_primitive_render_scene() {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData red;
    red.name = "red-material";
    red.base_color_factor[0] = 1.0f;
    red.base_color_factor[1] = 0.05f;
    red.base_color_factor[2] = 0.05f;
    red.double_sided = true;
    scene.materials.push_back(red);

    pulp::scene::MaterialData green;
    green.name = "green-material";
    green.base_color_factor[0] = 0.05f;
    green.base_color_factor[1] = 1.0f;
    green.base_color_factor[2] = 0.05f;
    green.double_sided = true;
    scene.materials.push_back(green);

    pulp::scene::PrimitiveData left;
    left.positions = {
        -2.0f, -1.0f, 0.0f,
        -0.4f, -1.0f, 0.0f,
        -1.2f,  1.0f, 0.0f,
    };
    left.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    left.indices = {0, 1, 2};
    left.material = 0;

    pulp::scene::PrimitiveData right;
    right.positions = {
         0.4f, -1.0f, 0.0f,
         2.0f, -1.0f, 0.0f,
         1.2f,  1.0f, 0.0f,
    };
    right.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    right.indices = {0, 1, 2};
    right.material = 1;

    pulp::scene::MeshData mesh;
    mesh.name = "mixed-material-mesh";
    mesh.primitives.push_back(std::move(left));
    mesh.primitives.push_back(std::move(right));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_directional_light_render_scene() {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = "white-lit-material";
    material.base_color_factor[0] = 0.9f;
    material.base_color_factor[1] = 0.9f;
    material.base_color_factor[2] = 0.9f;
    material.double_sided = true;
    scene.materials.push_back(material);

    pulp::scene::LightData light;
    light.name = "green-key";
    light.type = pulp::scene::LightData::Type::directional;
    light.color[0] = 0.05f;
    light.color[1] = 1.0f;
    light.color[2] = 0.05f;
    light.intensity = 1.0f;
    scene.lights.push_back(light);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "directional-light-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData mesh_node;
    mesh_node.name = "mesh";
    mesh_node.mesh = 0;
    scene.nodes.push_back(std::move(mesh_node));
    scene.root_nodes.push_back(0);

    pulp::scene::NodeData light_node;
    light_node.name = "light";
    light_node.light = 0;
    scene.nodes.push_back(std::move(light_node));
    scene.root_nodes.push_back(1);

    return scene;
}

pulp::scene::SceneData make_directional_light_direction_render_scene(
    bool rotated_away) {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = "directional-light-direction-material";
    material.base_color_factor[0] = 0.9f;
    material.base_color_factor[1] = 0.9f;
    material.base_color_factor[2] = 0.9f;
    material.double_sided = true;
    scene.materials.push_back(material);

    pulp::scene::LightData light;
    light.name = "white-key";
    light.type = pulp::scene::LightData::Type::directional;
    light.color[0] = 1.0f;
    light.color[1] = 1.0f;
    light.color[2] = 1.0f;
    light.intensity = 1.0f;
    scene.lights.push_back(light);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -0.65f, -0.65f, 0.0f,
         0.65f, -0.65f, 0.0f,
         0.0f,   0.65f, 0.0f,
    };
    primitive.normals = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "directional-light-direction-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData mesh_node;
    mesh_node.name = "mesh";
    mesh_node.mesh = 0;
    scene.nodes.push_back(std::move(mesh_node));
    scene.root_nodes.push_back(0);

    pulp::scene::NodeData light_node;
    light_node.name = "directional-light";
    light_node.light = 0;
    if (rotated_away) {
        light_node.rotation[1] = 1.0f;
        light_node.rotation[3] = 0.0f;
    }
    scene.nodes.push_back(std::move(light_node));
    scene.root_nodes.push_back(1);

    return scene;
}

pulp::scene::SceneData make_deferred_punctual_light_render_scene(
    bool attach_point_light = true,
    bool attach_spot_light = true) {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = "punctual-light-deferred-material";
    material.base_color_factor[0] = 0.85f;
    material.base_color_factor[1] = 0.85f;
    material.base_color_factor[2] = 0.85f;
    material.double_sided = true;
    scene.materials.push_back(material);

    pulp::scene::LightData point_light;
    point_light.name = "deferred-point";
    point_light.type = pulp::scene::LightData::Type::point;
    point_light.color[0] = 1.0f;
    point_light.color[1] = 0.2f;
    point_light.color[2] = 0.2f;
    point_light.intensity = 2.0f;
    point_light.range = 6.0f;
    scene.lights.push_back(point_light);

    pulp::scene::LightData spot_light;
    spot_light.name = "deferred-spot";
    spot_light.type = pulp::scene::LightData::Type::spot;
    spot_light.color[0] = 0.2f;
    spot_light.color[1] = 0.2f;
    spot_light.color[2] = 1.0f;
    spot_light.intensity = 3.0f;
    spot_light.range = 8.0f;
    spot_light.inner_cone_angle = 0.2f;
    spot_light.outer_cone_angle = 0.7f;
    scene.lights.push_back(spot_light);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "punctual-light-deferred-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData mesh_node;
    mesh_node.name = "mesh";
    mesh_node.mesh = 0;
    scene.nodes.push_back(std::move(mesh_node));
    scene.root_nodes.push_back(0);

    if (attach_point_light) {
        pulp::scene::NodeData point_node;
        point_node.name = "point-light";
        point_node.light = 0;
        point_node.translation[0] = 1.0f;
        point_node.translation[2] = 2.0f;
        scene.nodes.push_back(std::move(point_node));
        scene.root_nodes.push_back(
            static_cast<uint32_t>(scene.nodes.size() - 1u));
    }

    if (attach_spot_light) {
        pulp::scene::NodeData spot_node;
        spot_node.name = "spot-light";
        spot_node.light = 1;
        spot_node.translation[0] = -1.0f;
        spot_node.translation[2] = 2.0f;
        scene.nodes.push_back(std::move(spot_node));
        scene.root_nodes.push_back(
            static_cast<uint32_t>(scene.nodes.size() - 1u));
    }

    return scene;
}

pulp::scene::SceneData make_point_light_range_render_scene(float range) {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = "point-range-material";
    material.base_color_factor[0] = 0.85f;
    material.base_color_factor[1] = 0.85f;
    material.base_color_factor[2] = 0.85f;
    material.double_sided = true;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "point-range-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData mesh_node;
    mesh_node.name = "mesh";
    mesh_node.mesh = 0;
    scene.nodes.push_back(std::move(mesh_node));
    scene.root_nodes.push_back(0);

    pulp::scene::LightData point_light;
    point_light.name = "point-range";
    point_light.type = pulp::scene::LightData::Type::point;
    point_light.color[0] = 1.0f;
    point_light.color[1] = 0.25f;
    point_light.color[2] = 0.25f;
    point_light.intensity = 2.5f;
    point_light.range = range;
    scene.lights.push_back(point_light);

    pulp::scene::NodeData point_node;
    point_node.name = "point-light";
    point_node.light = 0;
    point_node.translation[2] = 2.0f;
    scene.nodes.push_back(std::move(point_node));
    scene.root_nodes.push_back(static_cast<uint32_t>(scene.nodes.size() - 1u));

    return scene;
}

pulp::scene::SceneData make_perspective_camera_render_scene(bool with_camera) {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = "camera-scale-material";
    material.base_color_factor[0] = 0.85f;
    material.base_color_factor[1] = 0.85f;
    material.base_color_factor[2] = 0.85f;
    material.double_sided = true;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -0.65f, -0.65f, 0.0f,
         0.65f, -0.65f, 0.0f,
         0.0f,   0.65f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "camera-scale-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData mesh_node;
    mesh_node.name = "mesh";
    mesh_node.mesh = 0;
    scene.nodes.push_back(std::move(mesh_node));
    scene.root_nodes.push_back(0);

    if (with_camera) {
        pulp::scene::CameraData camera;
        camera.name = "perspective-camera";
        camera.projection = pulp::scene::CameraData::Projection::perspective;
        camera.yfov = 0.55f;
        camera.znear = 0.1f;
        scene.cameras.push_back(camera);

        pulp::scene::NodeData camera_node;
        camera_node.name = "camera";
        camera_node.camera = 0;
        scene.nodes.push_back(std::move(camera_node));
        scene.root_nodes.push_back(1);
    }

    return scene;
}

pulp::scene::SceneData make_transformed_camera_light_render_scene() {
    auto scene = make_perspective_camera_render_scene(true);

    pulp::scene::LightData light;
    light.name = "rotated-key";
    light.type = pulp::scene::LightData::Type::directional;
    light.color[0] = 0.8f;
    light.color[1] = 0.8f;
    light.color[2] = 1.0f;
    light.intensity = 1.2f;
    scene.lights.push_back(light);

    for (auto& node : scene.nodes) {
        if (node.camera == 0) {
            node.translation[2] = 4.0f;
            node.rotation[1] = 0.258819f;
            node.rotation[3] = 0.965926f;
        }
    }

    pulp::scene::NodeData light_node;
    light_node.name = "rotated-light";
    light_node.light = 0;
    light_node.rotation[0] = 0.382683f;
    light_node.rotation[3] = 0.92388f;
    scene.nodes.push_back(std::move(light_node));
    scene.root_nodes.push_back(static_cast<uint32_t>(scene.nodes.size() - 1u));

    return scene;
}

pulp::scene::SceneData make_translated_camera_render_scene(float camera_x) {
    auto scene = make_perspective_camera_render_scene(true);
    REQUIRE_FALSE(scene.meshes.empty());
    REQUIRE_FALSE(scene.meshes[0].primitives.empty());
    scene.meshes[0].primitives[0].positions = {
        -0.35f, -0.35f, 0.0f,
         0.35f, -0.35f, 0.0f,
         0.0f,   0.35f, 0.0f,
    };
    for (auto& node : scene.nodes) {
        if (node.camera == 0) {
            node.translation[0] = camera_x;
        }
    }
    return scene;
}

pulp::scene::SceneData make_rotated_camera_render_scene(float camera_yaw_half_sin,
                                                        float camera_yaw_half_cos) {
    auto scene = make_translated_camera_render_scene(-0.25f);
    for (auto& node : scene.nodes) {
        if (node.camera == 0) {
            node.rotation[1] = camera_yaw_half_sin;
            node.rotation[3] = camera_yaw_half_cos;
        }
    }
    return scene;
}

pulp::scene::SceneData make_matrix_camera_render_scene(bool rotated) {
    auto scene = make_translated_camera_render_scene(-0.25f);
    for (auto& node : scene.nodes) {
        if (node.camera == 0) {
            node.has_matrix_transform = true;
            for (size_t i = 0; i < 16; ++i) {
                node.matrix[i] = 0.0f;
            }
            node.matrix[0] = 1.0f;
            node.matrix[5] = 1.0f;
            node.matrix[10] = 1.0f;
            node.matrix[15] = 1.0f;
            node.matrix[12] = -0.25f;
            if (rotated) {
                constexpr float c = 0.70710677f;
                constexpr float s = 0.70710677f;
                node.matrix[0] = c;
                node.matrix[2] = -s;
                node.matrix[8] = s;
                node.matrix[10] = c;
            }
        }
    }
    return scene;
}

pulp::scene::SceneData make_camera_metadata_render_scene() {
    auto scene = make_perspective_camera_render_scene(true);
    REQUIRE_FALSE(scene.cameras.empty());
    scene.cameras[0].aspect_ratio = 1.75f;
    scene.cameras[0].znear = 0.25f;
    scene.cameras[0].zfar = 64.0f;
    return scene;
}

pulp::scene::SceneData make_camera_aspect_ratio_render_scene(float aspect_ratio) {
    auto scene = make_perspective_camera_render_scene(true);
    REQUIRE_FALSE(scene.cameras.empty());
    scene.cameras[0].aspect_ratio = aspect_ratio;
    REQUIRE_FALSE(scene.meshes.empty());
    REQUIRE_FALSE(scene.meshes[0].primitives.empty());
    scene.meshes[0].primitives[0].positions = {
        -0.45f, -0.45f, 0.0f,
         0.45f, -0.45f, 0.0f,
         0.0f,   0.45f, 0.0f,
    };
    return scene;
}

pulp::scene::SceneData make_orthographic_camera_render_scene(float ymag) {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = "orthographic-camera-scale-material";
    material.base_color_factor[0] = 0.85f;
    material.base_color_factor[1] = 0.85f;
    material.base_color_factor[2] = 0.85f;
    material.double_sided = true;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -0.65f, -0.65f, 0.0f,
         0.65f, -0.65f, 0.0f,
         0.0f,   0.65f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "orthographic-camera-scale-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData mesh_node;
    mesh_node.name = "mesh";
    mesh_node.mesh = 0;
    scene.nodes.push_back(std::move(mesh_node));
    scene.root_nodes.push_back(0);

    pulp::scene::CameraData camera;
    camera.name = "orthographic-camera";
    camera.projection = pulp::scene::CameraData::Projection::orthographic;
    camera.xmag = ymag;
    camera.ymag = ymag;
    camera.znear = 0.1f;
    camera.zfar = 10.0f;
    scene.cameras.push_back(camera);

    pulp::scene::NodeData camera_node;
    camera_node.name = "camera";
    camera_node.camera = 0;
    scene.nodes.push_back(std::move(camera_node));
    scene.root_nodes.push_back(1);

    return scene;
}

pulp::scene::SceneData make_transform_animation_render_scene(float initial_x = 0.35f) {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = "animated-transform-material";
    material.base_color_factor[0] = 0.85f;
    material.base_color_factor[1] = 0.85f;
    material.base_color_factor[2] = 0.85f;
    material.double_sided = true;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "animated-transform-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "animated-node";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    pulp::scene::NodeData anchor_node;
    anchor_node.name = "static-anchor-node";
    anchor_node.mesh = 0;
    scene.nodes.push_back(std::move(anchor_node));
    scene.root_nodes.push_back(1);

    pulp::scene::AnimationSamplerData sampler;
    sampler.input_times = {0.0f, 1.0f};
    sampler.output_values = {
        initial_x, 0.0f, 0.0f,
        0.35f, 0.0f, 0.0f,
    };
    sampler.output_components = 3;

    pulp::scene::AnimationData animation;
    animation.name = "translation-only";
    animation.samplers.push_back(std::move(sampler));
    animation.channels.push_back(pulp::scene::AnimationChannelData{
        0,
        0,
        pulp::scene::AnimationChannelData::Path::translation,
    });
    scene.animations.push_back(std::move(animation));

    return scene;
}

pulp::scene::SceneData make_rotation_animation_render_scene() {
    auto scene = make_transform_animation_render_scene(0.0f);
    scene.animations.clear();

    pulp::scene::AnimationSamplerData sampler;
    sampler.input_times = {0.0f, 1.0f};
    sampler.output_components = 4;
    sampler.interpolation =
        pulp::scene::AnimationSamplerData::Interpolation::cubic_spline;
    sampler.output_values = {
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.7071068f, 0.7071068f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
    };

    pulp::scene::AnimationData animation;
    animation.name = "rotation-cubic-initial-pose";
    animation.samplers.push_back(std::move(sampler));
    animation.channels.push_back(pulp::scene::AnimationChannelData{
        0,
        0,
        pulp::scene::AnimationChannelData::Path::rotation,
    });
    scene.animations.push_back(std::move(animation));
    return scene;
}

pulp::scene::SceneData make_scale_animation_render_scene() {
    auto scene = make_transform_animation_render_scene(0.0f);
    scene.animations.clear();

    pulp::scene::AnimationSamplerData sampler;
    sampler.input_times = {0.0f, 1.0f};
    sampler.output_values = {
        1.45f, 0.7f, 1.0f,
        1.0f, 1.0f, 1.0f,
    };
    sampler.output_components = 3;

    pulp::scene::AnimationData animation;
    animation.name = "scale-initial-pose";
    animation.samplers.push_back(std::move(sampler));
    animation.channels.push_back(pulp::scene::AnimationChannelData{
        0,
        0,
        pulp::scene::AnimationChannelData::Path::scale,
    });
    scene.animations.push_back(std::move(animation));
    return scene;
}

pulp::scene::SceneData make_unsupported_feature_deferred_render_scene() {
    auto scene = make_perspective_camera_render_scene(true);
    scene.unsupported_features.push_back(pulp::scene::UnsupportedFeatureData{
        "Skinning",
        "Node references a glTF skin, but native skinning is not implemented in this slice.",
        "SkinnedNode",
    });
    scene.unsupported_features.push_back(pulp::scene::UnsupportedFeatureData{
        "MorphTargets",
        "Primitive morph targets are present, but native morph target evaluation is not implemented in this slice.",
        "MorphedMesh",
    });
    scene.unsupported_features.push_back(pulp::scene::UnsupportedFeatureData{
        "GpuInstancing",
        "EXT_mesh_gpu_instancing attributes are present, but native instanced rendering is not implemented in this slice.",
        "InstancedNode",
    });
    return scene;
}

pulp::scene::SceneData make_advanced_material_extension_render_scene() {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = "physical-extension-material";
    material.base_color_factor[0] = 0.75f;
    material.base_color_factor[1] = 0.82f;
    material.base_color_factor[2] = 0.92f;
    material.double_sided = true;
    material.advanced_material_extensions.push_back("KHR_materials_clearcoat");
    material.advanced_material_extensions.push_back("KHR_materials_transmission");
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "physical-extension-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "physical-extension-node";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_unlit_render_scene(bool unlit) {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = unlit ? "unlit-material" : "lit-material";
    material.unlit = unlit;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "unlit-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

std::vector<uint8_t> make_alpha_checker_png() {
    return {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xb6, 0x0d, 0x24, 0x00, 0x00, 0x00,
        0x11, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0x0f, 0x01, 0x0c,
        0x0c, 0x20, 0x02, 0xc4, 0x02, 0x00, 0x80, 0x91, 0x0d, 0xf3, 0x7e, 0xc1,
        0xff, 0xde, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
        0x60, 0x82,
    };
}

std::vector<uint8_t> make_blue_pixel_png() {
    return {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
        0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x60, 0x60, 0xf8, 0xff,
        0x1f, 0x00, 0x03, 0x02, 0x01, 0xff, 0xe6, 0x77, 0x0b, 0xae, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
    };
}

std::vector<uint8_t> make_black_pixel_png() {
    return {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
        0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x60, 0x60, 0x60, 0xf8,
        0x0f, 0x00, 0x01, 0x04, 0x01, 0x00, 0x5f, 0xe5, 0xc3, 0x4b, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
    };
}

std::vector<uint8_t> make_black_blue_columns_png() {
    HeadlessSurface::Rgba rgba;
    rgba.width = 2;
    rgba.height = 2;
    rgba.pixels = {
        0, 0, 0, 255,    0, 0, 255, 255,
        0, 0, 0, 255,    0, 0, 255, 255,
    };
    std::string error;
    auto png = HeadlessSurface::encode_png(rgba, &error);
    REQUIRE_FALSE(png.empty());
    return png;
}

std::vector<uint8_t> make_black_white_columns_png() {
    HeadlessSurface::Rgba rgba;
    rgba.width = 2;
    rgba.height = 2;
    rgba.pixels = {
        0, 0, 0, 255,      255, 255, 255, 255,
        0, 0, 0, 255,      255, 255, 255, 255,
    };
    std::string error;
    auto png = HeadlessSurface::encode_png(rgba, &error);
    REQUIRE_FALSE(png.empty());
    return png;
}

std::vector<uint8_t> make_reverse_z_normal_png() {
    return {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
        0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x68, 0x68, 0x60, 0xf8,
        0x0f, 0x00, 0x04, 0x84, 0x02, 0x00, 0x0a, 0x2c, 0x99, 0xd6, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
    };
}

std::vector<uint8_t> make_tilted_normal_png() {
    HeadlessSurface::Rgba rgba;
    rgba.width = 1;
    rgba.height = 1;
    rgba.pixels = {255, 96, 192, 255};
    std::string error;
    auto png = HeadlessSurface::encode_png(rgba, &error);
    REQUIRE_FALSE(png.empty());
    return png;
}

std::vector<uint8_t> make_external_renderable_gltf_bin() {
    std::vector<uint8_t> bin;
    for (float value : {
             -1.0f, -1.0f, 0.0f,
              1.0f, -1.0f, 0.0f,
              0.0f,  1.0f, 0.0f,
         }) {
        append_f32(bin, value);
    }
    for (float value : {
             0.0f, 0.0f, 1.0f,
             0.0f, 0.0f, 1.0f,
             0.0f, 0.0f, 1.0f,
         }) {
        append_f32(bin, value);
    }
    for (float value : {
             0.0f, 0.0f,
             1.0f, 0.0f,
             0.5f, 1.0f,
         }) {
        append_f32(bin, value);
    }
    append_u16(bin, 0);
    append_u16(bin, 1);
    append_u16(bin, 2);
    pad4(bin);
    return bin;
}

std::string make_external_renderable_gltf_json(size_t bin_byte_length) {
    return R"({"asset":{"version":"2.0","generator":"pulp-test-renderer3d-external"},"buffers":[{"uri":"mesh.bin","byteLength":)" +
        std::to_string(bin_byte_length) +
        R"(}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":36,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":72,"byteLength":24,"target":34962},{"buffer":0,"byteOffset":96,"byteLength":6,"target":34963}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-1,-1,0],"max":[1,1,0]},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},{"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}],"images":[{"uri":"checker.png","mimeType":"image/png","name":"external-checker"}],"samplers":[{"name":"externalLinearClamp","magFilter":9729,"minFilter":9729,"wrapS":33071,"wrapT":33071}],"textures":[{"source":0,"sampler":0,"name":"externalCheckerTexture"}],"materials":[{"name":"external-mat","pbrMetallicRoughness":{"baseColorFactor":[0.8,0.5,0.25,1],"baseColorTexture":{"index":0}},"doubleSided":true}],"meshes":[{"name":"external-triangle","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0,"mode":4}]}],"nodes":[{"name":"external-root","mesh":0,"translation":[-1,0,0]}],"scenes":[{"nodes":[0]}],"scene":0})";
}

pulp::scene::SceneData make_alpha_mask_render_scene(bool masked) {
    pulp::scene::SceneData scene;

    pulp::scene::TextureData texture;
    texture.name = "alpha-checker";
    texture.mime_type = "image/png";
    texture.encoded_bytes = make_alpha_checker_png();
    scene.textures.push_back(std::move(texture));

    pulp::scene::MaterialData material;
    material.name = masked ? "masked-material" : "opaque-material";
    material.base_color_texture = 0;
    if (masked) {
        material.alpha_mode = pulp::scene::MaterialData::AlphaMode::mask;
        material.alpha_cutoff = 0.5f;
    }
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "alpha-mask-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_vertex_color_render_scene() {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = "vertex-color-material";
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.color0 = {
        1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "vertex-color-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_back_facing_render_scene(bool double_sided) {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = double_sided ? "double-sided-material" : "single-sided-material";
    material.double_sided = double_sided;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        0.5f, 1.0f,
        1.0f, 0.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "back-facing-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_alpha_blend_render_scene(bool blended) {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = blended ? "blend-material" : "opaque-material";
    material.base_color_factor[0] = 1.0f;
    material.base_color_factor[1] = 1.0f;
    material.base_color_factor[2] = 1.0f;
    material.base_color_factor[3] = blended ? 0.35f : 1.0f;
    if (blended) {
        material.alpha_mode = pulp::scene::MaterialData::AlphaMode::blend;
    }
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "alpha-blend-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_alpha_blend_over_opaque_render_scene() {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData red_blend;
    red_blend.name = "red-blend-front";
    red_blend.base_color_factor[0] = 1.0f;
    red_blend.base_color_factor[1] = 0.05f;
    red_blend.base_color_factor[2] = 0.05f;
    red_blend.base_color_factor[3] = 0.45f;
    red_blend.alpha_mode = pulp::scene::MaterialData::AlphaMode::blend;
    red_blend.double_sided = true;
    scene.materials.push_back(red_blend);

    pulp::scene::MaterialData green_opaque;
    green_opaque.name = "green-opaque-back";
    green_opaque.base_color_factor[0] = 0.05f;
    green_opaque.base_color_factor[1] = 1.0f;
    green_opaque.base_color_factor[2] = 0.05f;
    green_opaque.double_sided = true;
    scene.materials.push_back(green_opaque);

    pulp::scene::PrimitiveData blended_front;
    blended_front.positions = {
        -1.0f, -1.0f, 0.35f,
         1.0f, -1.0f, 0.35f,
         0.0f,  1.0f, 0.35f,
    };
    blended_front.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    blended_front.indices = {0, 1, 2};
    blended_front.material = 0;

    pulp::scene::PrimitiveData opaque_back;
    opaque_back.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    opaque_back.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    opaque_back.indices = {0, 1, 2};
    opaque_back.material = 1;

    pulp::scene::MeshData mesh;
    mesh.name = "alpha-blend-over-opaque";
    mesh.primitives.push_back(std::move(blended_front));
    mesh.primitives.push_back(std::move(opaque_back));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_alpha_blend_sorted_layers_render_scene() {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData red_front;
    red_front.name = "red-blend-front";
    red_front.base_color_factor[0] = 1.0f;
    red_front.base_color_factor[1] = 0.05f;
    red_front.base_color_factor[2] = 0.05f;
    red_front.base_color_factor[3] = 0.55f;
    red_front.alpha_mode = pulp::scene::MaterialData::AlphaMode::blend;
    red_front.double_sided = true;
    scene.materials.push_back(red_front);

    pulp::scene::MaterialData blue_back;
    blue_back.name = "blue-blend-back";
    blue_back.base_color_factor[0] = 0.05f;
    blue_back.base_color_factor[1] = 0.05f;
    blue_back.base_color_factor[2] = 1.0f;
    blue_back.base_color_factor[3] = 0.55f;
    blue_back.alpha_mode = pulp::scene::MaterialData::AlphaMode::blend;
    blue_back.double_sided = true;
    scene.materials.push_back(blue_back);

    pulp::scene::PrimitiveData front_submitted_first;
    front_submitted_first.positions = {
        -1.0f, -1.0f,  0.35f,
         1.0f, -1.0f,  0.35f,
         0.0f,  1.0f,  0.35f,
    };
    front_submitted_first.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    front_submitted_first.indices = {0, 1, 2};
    front_submitted_first.material = 0;

    pulp::scene::PrimitiveData back_submitted_second;
    back_submitted_second.positions = {
        -1.0f, -1.0f, -0.35f,
         1.0f, -1.0f, -0.35f,
         0.0f,  1.0f, -0.35f,
    };
    back_submitted_second.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    back_submitted_second.indices = {0, 1, 2};
    back_submitted_second.material = 1;

    pulp::scene::MeshData mesh;
    mesh.name = "alpha-blend-sorted-layers";
    mesh.primitives.push_back(std::move(front_submitted_first));
    mesh.primitives.push_back(std::move(back_submitted_second));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_emissive_render_scene(bool emissive,
                                                  float emissive_strength = 1.0f) {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = emissive ? "emissive-material" : "plain-material";
    material.base_color_factor[0] = 0.1f;
    material.base_color_factor[1] = 0.1f;
    material.base_color_factor[2] = 0.1f;
    if (emissive) {
        material.emissive_factor[2] = 0.65f;
        material.emissive_strength = emissive_strength;
    }
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "emissive-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_emissive_texture_render_scene() {
    pulp::scene::SceneData scene;

    pulp::scene::TextureData texture;
    texture.name = "blue-emissive";
    texture.mime_type = "image/png";
    texture.encoded_bytes = make_blue_pixel_png();
    scene.textures.push_back(std::move(texture));

    pulp::scene::MaterialData material;
    material.name = "emissive-texture-material";
    material.base_color_factor[0] = 0.05f;
    material.base_color_factor[1] = 0.05f;
    material.base_color_factor[2] = 0.05f;
    material.emissive_factor[0] = 1.0f;
    material.emissive_factor[1] = 1.0f;
    material.emissive_factor[2] = 1.0f;
    material.emissive_strength = 1.0f;
    material.emissive_texture = 0;
    material.double_sided = true;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "emissive-texture-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_emissive_texture_texcoord1_transform_render_scene() {
    pulp::scene::SceneData scene;

    pulp::scene::TextureData texture;
    texture.name = "black-blue-emissive";
    texture.mime_type = "image/png";
    texture.encoded_bytes = make_black_blue_columns_png();
    scene.textures.push_back(std::move(texture));

    pulp::scene::TextureSamplerData sampler;
    sampler.name = "nearest-clamp";
    sampler.mag_filter = pulp::scene::TextureSamplerData::Filter::nearest;
    sampler.min_filter = pulp::scene::TextureSamplerData::Filter::nearest;
    sampler.wrap_s = pulp::scene::TextureSamplerData::Wrap::clamp_to_edge;
    sampler.wrap_t = pulp::scene::TextureSamplerData::Wrap::clamp_to_edge;
    scene.texture_samplers.push_back(sampler);

    pulp::scene::MaterialData material;
    material.name = "emissive-texcoord1-transform-material";
    material.base_color_factor[0] = 0.02f;
    material.base_color_factor[1] = 0.02f;
    material.base_color_factor[2] = 0.02f;
    material.emissive_factor[0] = 1.0f;
    material.emissive_factor[1] = 1.0f;
    material.emissive_factor[2] = 1.0f;
    material.emissive_strength = 1.0f;
    material.emissive_texture = 0;
    material.emissive_sampler = 0;
    material.emissive_texcoord = 1;
    material.emissive_transform.enabled = true;
    material.emissive_transform.offset[0] = 0.5f;
    material.emissive_transform.offset[1] = 0.0f;
    material.double_sided = true;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.5f,
        0.0f, 0.5f,
        0.0f, 0.5f,
    };
    primitive.texcoord1 = {
        0.25f, 0.5f,
        0.25f, 0.5f,
        0.25f, 0.5f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "emissive-texcoord1-transform-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_metallic_roughness_factor_render_scene(
    float metallic,
    float roughness) {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = "metallic-roughness-factor-material";
    material.base_color_factor[0] = 0.8f;
    material.base_color_factor[1] = 0.8f;
    material.base_color_factor[2] = 0.8f;
    material.metallic_factor = metallic;
    material.roughness_factor = roughness;
    material.double_sided = true;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "metallic-roughness-factor-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_metallic_roughness_texture_render_scene() {
    auto scene = make_metallic_roughness_factor_render_scene(1.0f, 1.0f);

    pulp::scene::TextureData texture;
    texture.name = "black-metallic-roughness";
    texture.mime_type = "image/png";
    texture.encoded_bytes = make_black_pixel_png();
    scene.textures.push_back(std::move(texture));

    scene.materials[0].metallic_roughness_texture = 0;
    return scene;
}

pulp::scene::SceneData make_metallic_roughness_texcoord1_transform_render_scene(
    bool transformed) {
    auto scene = make_metallic_roughness_factor_render_scene(1.0f, 1.0f);

    pulp::scene::TextureData texture;
    texture.name = "black-blue-metallic-roughness";
    texture.mime_type = "image/png";
    texture.encoded_bytes = make_black_blue_columns_png();
    scene.textures.push_back(std::move(texture));

    pulp::scene::TextureSamplerData sampler;
    sampler.name = "nearest-clamp";
    sampler.mag_filter = pulp::scene::TextureSamplerData::Filter::nearest;
    sampler.min_filter = pulp::scene::TextureSamplerData::Filter::nearest;
    sampler.wrap_s = pulp::scene::TextureSamplerData::Wrap::clamp_to_edge;
    sampler.wrap_t = pulp::scene::TextureSamplerData::Wrap::clamp_to_edge;
    scene.texture_samplers.push_back(sampler);

    scene.materials[0].metallic_roughness_texture = 0;
    scene.materials[0].metallic_roughness_sampler = 0;
    scene.materials[0].metallic_roughness_texcoord = 1;
    scene.materials[0].metallic_roughness_transform.enabled = true;
    scene.materials[0].metallic_roughness_transform.offset[0] =
        transformed ? 0.5f : 0.0f;

    auto& primitive = scene.meshes[0].primitives[0];
    primitive.texcoord0 = {
        0.0f, 0.5f,
        0.0f, 0.5f,
        0.0f, 0.5f,
    };
    primitive.texcoord1 = {
        0.25f, 0.5f,
        0.25f, 0.5f,
        0.25f, 0.5f,
    };

    return scene;
}

pulp::scene::SceneData make_occlusion_texture_render_scene(
    float occlusion_strength = 1.0f) {
    auto scene = make_metallic_roughness_factor_render_scene(0.0f, 0.1f);

    pulp::scene::TextureData texture;
    texture.name = "black-occlusion";
    texture.mime_type = "image/png";
    texture.encoded_bytes = make_black_pixel_png();
    scene.textures.push_back(std::move(texture));

    scene.materials[0].occlusion_texture = 0;
    scene.materials[0].occlusion_strength = occlusion_strength;
    return scene;
}

pulp::scene::SceneData make_occlusion_texcoord1_transform_render_scene(
    bool transformed) {
    auto scene = make_metallic_roughness_factor_render_scene(0.0f, 0.1f);

    pulp::scene::TextureData texture;
    texture.name = "black-white-occlusion";
    texture.mime_type = "image/png";
    texture.encoded_bytes = make_black_white_columns_png();
    scene.textures.push_back(std::move(texture));

    pulp::scene::TextureSamplerData sampler;
    sampler.name = "nearest-clamp";
    sampler.mag_filter = pulp::scene::TextureSamplerData::Filter::nearest;
    sampler.min_filter = pulp::scene::TextureSamplerData::Filter::nearest;
    sampler.wrap_s = pulp::scene::TextureSamplerData::Wrap::clamp_to_edge;
    sampler.wrap_t = pulp::scene::TextureSamplerData::Wrap::clamp_to_edge;
    scene.texture_samplers.push_back(sampler);

    scene.materials[0].occlusion_texture = 0;
    scene.materials[0].occlusion_sampler = 0;
    scene.materials[0].occlusion_texcoord = 1;
    scene.materials[0].occlusion_transform.enabled = true;
    scene.materials[0].occlusion_transform.offset[0] =
        transformed ? 0.5f : 0.0f;

    auto& primitive = scene.meshes[0].primitives[0];
    primitive.texcoord0 = {
        0.0f, 0.5f,
        0.0f, 0.5f,
        0.0f, 0.5f,
    };
    primitive.texcoord1 = {
        0.25f, 0.5f,
        0.25f, 0.5f,
        0.25f, 0.5f,
    };

    return scene;
}

pulp::scene::SceneData make_geometry_normal_render_scene(bool toward_light) {
    pulp::scene::SceneData scene;

    pulp::scene::MaterialData material;
    material.name = toward_light ? "front-normal-material" : "back-normal-material";
    material.base_color_factor[0] = 0.8f;
    material.base_color_factor[1] = 0.8f;
    material.base_color_factor[2] = 0.8f;
    material.double_sided = true;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    const float nz = toward_light ? 1.0f : -1.0f;
    primitive.normals = {
        0.0f, 0.0f, nz,
        0.0f, 0.0f, nz,
        0.0f, 0.0f, nz,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "normal-lit-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_normal_texture_deferred_render_scene() {
    pulp::scene::SceneData scene;

    pulp::scene::TextureData normal_texture;
    normal_texture.name = "normal-map-placeholder";
    normal_texture.mime_type = "image/png";
    normal_texture.encoded_bytes = {0x89, 0x50, 0x4e, 0x47};
    scene.textures.push_back(std::move(normal_texture));

    pulp::scene::MaterialData material;
    material.name = "normal-map-material";
    material.base_color_factor[0] = 0.7f;
    material.base_color_factor[1] = 0.7f;
    material.base_color_factor[2] = 0.7f;
    material.double_sided = true;
    material.normal_texture = 0;
    material.normal_scale = 0.6f;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.normals = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
    };
    primitive.tangents = {
        1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "normal-map-deferred-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_normal_texture_render_scene(
    float normal_scale = 1.0f,
    bool tilted_normal = false) {
    pulp::scene::SceneData scene;

    pulp::scene::TextureData normal_texture;
    normal_texture.name = tilted_normal ? "tilted-normal" : "reverse-z-normal";
    normal_texture.mime_type = "image/png";
    normal_texture.encoded_bytes = tilted_normal
        ? make_tilted_normal_png()
        : make_reverse_z_normal_png();
    scene.textures.push_back(std::move(normal_texture));

    pulp::scene::MaterialData material;
    material.name = "sampled-normal-map-material";
    material.base_color_factor[0] = 0.8f;
    material.base_color_factor[1] = 0.8f;
    material.base_color_factor[2] = 0.8f;
    material.double_sided = true;
    material.normal_texture = 0;
    material.normal_scale = normal_scale;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.normals = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
    };
    primitive.tangents = {
        1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "sampled-normal-map-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

pulp::scene::SceneData make_normal_texture_derived_tangent_render_scene() {
    auto scene = make_normal_texture_render_scene();
    REQUIRE_FALSE(scene.meshes.empty());
    REQUIRE_FALSE(scene.meshes[0].primitives.empty());
    scene.meshes[0].primitives[0].tangents.clear();
    scene.materials[0].normal_scale = 0.8f;
    return scene;
}

pulp::scene::SceneData make_pbr_texture_slots_deferred_render_scene() {
    pulp::scene::SceneData scene;

    for (const char* name : {
             "metallic-roughness-placeholder",
             "normal-placeholder",
             "occlusion-placeholder",
             "emissive-placeholder",
         }) {
        pulp::scene::TextureData texture;
        texture.name = name;
        texture.mime_type = "image/png";
        texture.encoded_bytes = make_black_pixel_png();
        scene.textures.push_back(std::move(texture));
    }

    pulp::scene::MaterialData material;
    material.name = "deferred-pbr-texture-slots-material";
    material.base_color_factor[0] = 0.75f;
    material.base_color_factor[1] = 0.75f;
    material.base_color_factor[2] = 0.75f;
    material.double_sided = true;
    material.metallic_roughness_texture = 0;
    material.normal_texture = 1;
    material.occlusion_texture = 2;
    material.emissive_texture = 3;
    material.metallic_roughness_texcoord = 1;
    material.normal_texcoord = 1;
    material.occlusion_texcoord = 1;
    material.emissive_texcoord = 1;
    material.metallic_roughness_transform.enabled = true;
    material.metallic_roughness_transform.offset[0] = 0.2f;
    material.metallic_roughness_transform.scale[0] = 0.75f;
    material.normal_transform.enabled = true;
    material.normal_transform.rotation = 0.1f;
    material.occlusion_transform.enabled = true;
    material.occlusion_transform.offset[1] = 0.25f;
    material.emissive_transform.enabled = true;
    material.emissive_transform.scale[1] = 1.25f;
    material.normal_scale = 0.6f;
    material.occlusion_strength = 0.4f;
    material.emissive_factor[0] = 0.1f;
    material.emissive_factor[1] = 0.05f;
    material.emissive_factor[2] = 0.0f;
    scene.materials.push_back(material);

    pulp::scene::PrimitiveData primitive;
    primitive.positions = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    primitive.normals = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
    };
    primitive.texcoord0 = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.5f, 1.0f,
    };
    primitive.texcoord1 = {
        0.5f, 0.5f,
        0.5f, 0.5f,
        0.5f, 0.5f,
    };
    primitive.indices = {0, 1, 2};
    primitive.material = 0;

    pulp::scene::MeshData mesh;
    mesh.name = "deferred-pbr-texture-slots-triangle";
    mesh.primitives.push_back(std::move(primitive));
    scene.meshes.push_back(std::move(mesh));

    pulp::scene::NodeData node;
    node.name = "root";
    node.mesh = 0;
    scene.nodes.push_back(std::move(node));
    scene.root_nodes.push_back(0);

    return scene;
}

} // namespace

TEST_CASE("Renderer3D hardcoded textured cube renders offscreen", "[render][scene3d][gpu]") {
    HardcodedCubeRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_hardcoded_textured_cube(config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.width == config.width);
    REQUIRE(result.height == config.height);
    REQUIRE(result.color_target_allocated);
    REQUIRE(result.depth_target_allocated);
    REQUIRE(result.vertex_buffer_uploaded);
    REQUIRE(result.index_buffer_uploaded);
    REQUIRE(result.uniform_buffer_uploaded);
    REQUIRE(result.texture_uploaded);
    REQUIRE(result.command_submitted);
    REQUIRE(result.readback_completed);
    REQUIRE(result.adapter_info_available);
    REQUIRE_FALSE(result.adapter_backend.empty());
    REQUIRE_FALSE(result.adapter_backend_type.empty());
    REQUIRE_FALSE(result.adapter_name.empty());
    REQUIRE(result.rgba.size() == static_cast<size_t>(config.width) * config.height * 4u);
    REQUIRE(result.distinct_color_count > 1);
    REQUIRE(result.non_transparent_pixel_count > 0);
    auto foreground = find_foreground_region(result.rgba, config.width, config.height);
    REQUIRE(foreground.pixel_count > 1500);
    REQUIRE(foreground.min_x > 3);
    REQUIRE(foreground.max_x < config.width - 3);
    REQUIRE(foreground.min_y > 3);
    REQUIRE(foreground.max_y < config.height - 3);
    REQUIRE_FALSE(result.png.empty());
    REQUIRE(result.success);

    const HeadlessSurface::Rgba rgba{
        result.rgba,
        result.width,
        result.height,
    };
    if (is_mac_metal_adapter(result)) {
        REQUIRE(HeadlessSurface::rgba_fingerprint(rgba) ==
                kMacMetalHardcodedCubeFingerprint);
    } else {
        SUCCEED("Renderer fingerprint golden is scoped to macOS Metal; adapter was "
                << result.adapter_backend_type << " / " << result.adapter_name);
    }

    auto out = std::filesystem::temp_directory_path() /
        "pulp-renderer3d-hardcoded-cube.png";
    std::ofstream png(out, std::ios::binary);
    png.write(reinterpret_cast<const char*>(result.png.data()),
              static_cast<std::streamsize>(result.png.size()));
    REQUIRE(png.good());
}

TEST_CASE("Renderer3D can request the Dawn fallback adapter for golden probes",
          "[render][scene3d][gpu][adapter]") {
    HardcodedCubeRenderConfig config;
    config.width = 64;
    config.height = 64;
    config.force_fallback_adapter = true;

    auto result = Renderer3D::render_hardcoded_textured_cube(config);
    REQUIRE(result.fallback_adapter_requested);
    if (!result.gpu_available) {
        SUCCEED("Dawn fallback adapter unavailable in this environment: "
                << result.error);
        return;
    }

    INFO(result.error);
    INFO("adapter_backend_type=" << result.adapter_backend_type);
    INFO("adapter_name=" << result.adapter_name);
    REQUIRE(result.success);
    REQUIRE(result.adapter_info_available);
    REQUIRE_FALSE(result.adapter_backend.empty());
    REQUIRE_FALSE(result.adapter_backend_type.empty());
    REQUIRE_FALSE(result.adapter_name.empty());
    REQUIRE(result.width == config.width);
    REQUIRE(result.height == config.height);
    REQUIRE(result.readback_completed);
    REQUIRE(result.distinct_color_count > 1);
    REQUIRE(result.non_transparent_pixel_count > 0);
}

TEST_CASE("GpuSurface can request the Dawn null backend for API-only probes",
          "[render][scene3d][gpu][adapter]") {
    auto gpu = GpuSurface::create_dawn();
    if (!gpu) {
        SUCCEED("Dawn/WebGPU unavailable in this environment");
        return;
    }

    GpuSurface::Config config;
    config.width = 16;
    config.height = 16;
    config.native_surface_handle = nullptr;
    config.backend_preference =
        GpuSurface::AdapterBackendPreference::null_backend;

    if (!gpu->initialize(config)) {
        SUCCEED("Dawn null backend unavailable in this environment");
        return;
    }

    const auto info = gpu->adapter_info();
    REQUIRE(info.available);
    REQUIRE(info.backend_type == "Null");
    REQUIRE(gpu->is_initialized());
    REQUIRE_FALSE(gpu->has_surface());
    REQUIRE(gpu->width() == config.width);
    REQUIRE(gpu->height() == config.height);
}

TEST_CASE("Renderer3D can request Dawn null backend for API-only probes",
          "[render][scene3d][gpu][adapter]") {
    HardcodedCubeRenderConfig config;
    config.width = 32;
    config.height = 32;
    config.backend_preference =
        Renderer3DAdapterBackendPreference::null_backend;

    auto result = Renderer3D::render_hardcoded_textured_cube(config);
    REQUIRE(result.null_backend_requested);
    if (!result.gpu_available) {
        SUCCEED("Dawn null backend unavailable in this environment: "
                << result.error);
        return;
    }

    INFO(result.error);
    INFO("adapter_backend_type=" << result.adapter_backend_type);
    INFO("adapter_name=" << result.adapter_name);
    REQUIRE(result.adapter_info_available);
    REQUIRE(result.adapter_backend_type == "Null");
    REQUIRE(result.width == config.width);
    REQUIRE(result.height == config.height);
    REQUIRE_FALSE(result.fallback_adapter_requested);
}

TEST_CASE("Renderer3D renders parsed SceneData offscreen", "[render][scene3d][gpu]") {
    auto path = write_temp_file("pulp-renderer3d-scenedata.glb",
                                make_renderable_textured_glb());
    auto loaded = pulp::scene::load_gltf_scene(path);
    INFO(loaded.error);
    REQUIRE(loaded.success);

    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(loaded.scene, config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.width == config.width);
    REQUIRE(result.height == config.height);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.primitive_count == 1);
    REQUIRE(result.color_target_allocated);
    REQUIRE(result.depth_target_allocated);
    REQUIRE(result.vertex_buffer_uploaded);
    REQUIRE(result.index_buffer_uploaded);
    REQUIRE(result.uniform_buffer_uploaded);
    REQUIRE(result.texture_uploaded);
    REQUIRE(result.texture_decoded);
    REQUIRE_FALSE(result.fallback_texture_used);
    REQUIRE(result.base_color_texture_srgb_applied);
    REQUIRE(result.texture_sampler_applied);
    REQUIRE(result.texture_sampler_clamp_s);
    REQUIRE(result.texture_sampler_clamp_t);
    REQUIRE(result.texture_sampler_linear);
    REQUIRE(result.base_color_factor_applied);
    REQUIRE(result.command_submitted);
    REQUIRE(result.readback_completed);
    REQUIRE(result.adapter_info_available);
    REQUIRE_FALSE(result.adapter_backend.empty());
    REQUIRE_FALSE(result.adapter_backend_type.empty());
    REQUIRE_FALSE(result.adapter_name.empty());
    REQUIRE(result.rgba.size() == static_cast<size_t>(config.width) * config.height * 4u);
    REQUIRE(result.distinct_color_count > 1);
    REQUIRE(result.non_transparent_pixel_count > 0);
    auto foreground = find_foreground_region(result.rgba, config.width, config.height);
    REQUIRE(foreground.pixel_count > 1200);
    REQUIRE(foreground.min_x > 8);
    REQUIRE(foreground.max_x < config.width - 8);
    REQUIRE(foreground.min_y > 8);
    REQUIRE(foreground.max_y < config.height - 8);
    REQUIRE_FALSE(result.png.empty());
    REQUIRE(result.success);

    auto out = std::filesystem::temp_directory_path() /
        "pulp-renderer3d-scenedata.png";
    std::ofstream png(out, std::ios::binary);
    png.write(reinterpret_cast<const char*>(result.png.data()),
              static_cast<std::streamsize>(result.png.size()));
    REQUIRE(png.good());
}

TEST_CASE("Renderer3D renders generated textured cube GLB",
          "[render][scene3d][gpu][gltf]") {
    auto path = write_temp_file("pulp-renderer3d-box-textured-like.glb",
                                make_renderable_textured_cube_glb());
    auto loaded = pulp::scene::load_gltf_scene(path);
    INFO(loaded.error);
    REQUIRE(loaded.success);
    REQUIRE(loaded.scene.meshes.size() == 1);
    REQUIRE(loaded.scene.meshes[0].primitives.size() == 1);
    REQUIRE(loaded.scene.meshes[0].primitives[0].positions.size() == 24u * 3u);
    REQUIRE(loaded.scene.meshes[0].primitives[0].normals.size() == 24u * 3u);
    REQUIRE(loaded.scene.meshes[0].primitives[0].texcoord0.size() == 24u * 2u);
    REQUIRE(loaded.scene.meshes[0].primitives[0].indices.size() == 36u);
    REQUIRE(loaded.scene.textures.size() == 1);
    REQUIRE_FALSE(loaded.scene.textures[0].encoded_bytes.empty());

    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(loaded.scene, config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.primitive_count == 1);
    REQUIRE(result.geometry_normals_applied);
    REQUIRE(result.texture_uploaded);
    REQUIRE(result.texture_decoded);
    REQUIRE_FALSE(result.fallback_texture_used);
    REQUIRE(result.base_color_texture_srgb_applied);
    REQUIRE(result.texture_sampler_applied);
    REQUIRE(result.readback_completed);
    REQUIRE(result.rgba.size() == static_cast<size_t>(config.width) * config.height * 4u);
    REQUIRE(result.distinct_color_count > 8);
    auto foreground = find_foreground_region(result.rgba, config.width, config.height);
    REQUIRE(foreground.pixel_count > 1500);
    REQUIRE(foreground.min_x > 4);
    REQUIRE(foreground.max_x < config.width - 4);
    REQUIRE(foreground.min_y > 0);
    REQUIRE(foreground.max_y < config.height);
    REQUIRE_FALSE(result.png.empty());

    auto out = std::filesystem::temp_directory_path() /
        "pulp-renderer3d-box-textured-like.png";
    std::ofstream png(out, std::ios::binary);
    png.write(reinterpret_cast<const char*>(result.png.data()),
              static_cast<std::streamsize>(result.png.size()));
    REQUIRE(png.good());
}

TEST_CASE("Renderer3D renders multiple parsed GLB mesh nodes",
          "[render][scene3d][gpu][gltf]") {
    auto path = write_temp_file("pulp-renderer3d-multi-node.glb",
                                make_multi_node_textured_glb());
    auto loaded = pulp::scene::load_gltf_scene(path);
    INFO(loaded.error);
    REQUIRE(loaded.success);
    REQUIRE(loaded.scene.nodes.size() == 2);
    REQUIRE(loaded.scene.root_nodes.size() == 2);
    REQUIRE(loaded.scene.meshes.size() == 1);
    REQUIRE(loaded.scene.meshes[0].primitives.size() == 1);
    REQUIRE(loaded.scene.textures.size() == 1);
    REQUIRE_FALSE(loaded.scene.textures[0].encoded_bytes.empty());

    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(loaded.scene, config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.primitive_count == 2);
    REQUIRE(result.pipeline_cache_entry_count == 1);
    REQUIRE(result.pipeline_cache_hit_count == 1);
    REQUIRE(result.texture_uploaded);
    REQUIRE(result.texture_decoded);
    REQUIRE_FALSE(result.fallback_texture_used);
    REQUIRE(result.base_color_texture_srgb_applied);
    REQUIRE(result.texture_sampler_applied);
    REQUIRE(result.command_submitted);
    REQUIRE(result.readback_completed);

    auto foreground = find_foreground_region(result.rgba,
                                             config.width,
                                             config.height);
    REQUIRE(foreground.pixel_count > 1000);
    REQUIRE(foreground.min_x > 2);
    REQUIRE(foreground.max_x < config.width - 2);
    REQUIRE(foreground_width(foreground) > 84);

    auto out = std::filesystem::temp_directory_path() /
        "pulp-renderer3d-multi-node.png";
    std::ofstream png(out, std::ios::binary);
    png.write(reinterpret_cast<const char*>(result.png.data()),
              static_cast<std::streamsize>(result.png.size()));
    REQUIRE(png.good());
}

TEST_CASE("Renderer3D renders official BoxTextured fixture",
          "[render][scene3d][gpu][gltf][fixture]") {
    const auto path = scene3d_fixture_path("BoxTextured/BoxTextured.glb");
    REQUIRE(std::filesystem::exists(path));

    auto loaded = pulp::scene::load_gltf_scene(path);
    INFO(loaded.error);
    REQUIRE(loaded.success);
    REQUIRE(loaded.scene.meshes.size() == 1);
    REQUIRE(loaded.scene.meshes[0].primitives.size() == 1);
    REQUIRE(loaded.scene.meshes[0].primitives[0].positions.size() == 24u * 3u);
    REQUIRE(loaded.scene.meshes[0].primitives[0].indices.size() == 36u);
    REQUIRE(loaded.scene.textures.size() == 1);
    REQUIRE_FALSE(loaded.scene.textures[0].encoded_bytes.empty());

    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(loaded.scene, config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.primitive_count == 1);
    REQUIRE(result.geometry_normals_applied);
    REQUIRE(result.texture_uploaded);
    REQUIRE(result.texture_decoded);
    REQUIRE_FALSE(result.fallback_texture_used);
    REQUIRE(result.base_color_texture_srgb_applied);
    REQUIRE(result.readback_completed);
    REQUIRE(result.adapter_info_available);
    REQUIRE_FALSE(result.adapter_backend.empty());
    REQUIRE_FALSE(result.adapter_backend_type.empty());
    REQUIRE_FALSE(result.adapter_name.empty());
    REQUIRE(result.rgba.size() == static_cast<size_t>(config.width) *
        config.height * 4u);
    REQUIRE(result.distinct_color_count > 8);
    auto foreground = find_foreground_region(result.rgba, config.width, config.height);
    REQUIRE(foreground.pixel_count > 1500);
    REQUIRE(foreground.min_x > 4);
    REQUIRE(foreground.max_x < config.width - 4);
    REQUIRE(foreground.min_y > 0);
    REQUIRE(foreground.max_y < config.height);
    REQUIRE_FALSE(result.png.empty());

    const HeadlessSurface::Rgba rgba{
        result.rgba,
        result.width,
        result.height,
    };
    if (is_mac_metal_adapter(result)) {
        REQUIRE(HeadlessSurface::rgba_fingerprint(rgba) ==
                kMacMetalBoxTexturedFixtureFingerprint);
    } else {
        SUCCEED("Renderer fingerprint golden is scoped to macOS Metal; adapter was "
                << result.adapter_backend_type << " / " << result.adapter_name);
    }

    auto out = std::filesystem::temp_directory_path() /
        "pulp-renderer3d-box-textured-official.png";
    std::ofstream png(out, std::ios::binary);
    png.write(reinterpret_cast<const char*>(result.png.data()),
              static_cast<std::streamsize>(result.png.size()));
    REQUIRE(png.good());
}

TEST_CASE("DRACO decoder unique-id overload rejects invalid data",
          "[render][draco][scene3d]") {
    DracoAttributeIds ids;
    ids.position = 7;
    ids.normal = 8;
    ids.texcoord0 = 9;
    ids.texcoord1 = 10;
    ids.tangent = 11;
    ids.color0 = 12;

    auto empty = decode_draco(nullptr, 0, ids);
    REQUIRE_FALSE(empty.success);
    REQUIRE_FALSE(empty.unique_id_attributes_applied);

    uint8_t garbage[] = {0, 1, 2, 3};
    auto invalid = decode_draco(garbage, 4, ids);
    REQUIRE_FALSE(invalid.success);
    REQUIRE_FALSE(invalid.unique_id_attributes_applied);
}

TEST_CASE("DRACO scene adapter reaches loader callback boundary",
          "[render][draco][scene3d]") {
    auto callback = make_scene_draco_decode_callback();
    pulp::scene::DracoAttributeIds ids;
    ids.position = 7;
    ids.normal = 8;
    ids.texcoord0 = 9;
    ids.texcoord1 = 10;
    ids.tangent = 11;
    ids.color0 = 12;

    uint8_t garbage[] = {0, 1, 2, 3};
    auto decoded = callback(garbage, 4, ids);
    REQUIRE_FALSE(decoded.success);
    REQUIRE(decoded.decoder_available == draco_decoder_available());
    REQUIRE(decoded.positions.empty());
    REQUIRE(decoded.indices.empty());

    const auto dir = std::filesystem::temp_directory_path() /
        "pulp-renderer3d-draco-adapter";
    std::filesystem::create_directories(dir);
    const auto bin_path = dir / "mesh.bin";
    std::vector<uint8_t> bin;
    append_f32(bin, -1.0f); append_f32(bin, -1.0f); append_f32(bin, 0.0f);
    append_f32(bin, 1.0f); append_f32(bin, -1.0f); append_f32(bin, 0.0f);
    append_f32(bin, 0.0f); append_f32(bin, 1.0f); append_f32(bin, 0.0f);
    append_u16(bin, 0); append_u16(bin, 1); append_u16(bin, 2);
    bin.push_back(0);
    bin.push_back(1);
    bin.push_back(2);
    bin.push_back(3);
    write_file(bin_path, bin);
    const auto gltf_path = dir / "draco.gltf";
    write_file(
        gltf_path,
        R"({"asset":{"version":"2.0","generator":"pulp-test-renderer3d-draco-adapter"},"extensionsUsed":["KHR_draco_mesh_compression"],"extensionsRequired":["KHR_draco_mesh_compression"],"buffers":[{"uri":"mesh.bin","byteLength":46}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":36,"byteLength":6,"target":34963},{"buffer":0,"byteOffset":42,"byteLength":4}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-1,-1,0],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],"meshes":[{"name":"compressed","primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":4,"extensions":{"KHR_draco_mesh_compression":{"bufferView":2,"attributes":{"POSITION":7,"NORMAL":8,"TEXCOORD_0":9,"TEXCOORD_1":10,"TANGENT":11,"COLOR_0":12}}}}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})");

    pulp::scene::LoadOptions options;
    options.draco_decode = make_scene_draco_decode_callback();
    auto result = pulp::scene::load_gltf_scene(gltf_path, options);

    REQUIRE_FALSE(result.success);
    REQUIRE(pulp::scene::has_error_diagnostics(result.scene.diagnostics));
    REQUIRE_FALSE(result.scene.diagnostics.empty());
    if (draco_decoder_available()) {
        REQUIRE(result.scene.diagnostics[0].code == "gltf.draco_decode_failed");
    } else {
        REQUIRE(result.scene.diagnostics[0].code == "gltf.draco_unavailable");
    }
}

#ifdef PULP_TEST_HAS_DRACO
TEST_CASE("DRACO decoder preserves glTF unique-id attributes from encoded mesh",
          "[render][draco][scene3d]") {
    draco::TriangleSoupMeshBuilder builder;
    builder.Start(1);
    const int position_id = builder.AddAttribute(
        draco::GeometryAttribute::POSITION, 3, draco::DT_FLOAT32);
    const int normal_id = builder.AddAttribute(
        draco::GeometryAttribute::NORMAL, 3, draco::DT_FLOAT32);
    const int texcoord0_id = builder.AddAttribute(
        draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32);
    const int texcoord1_id = builder.AddAttribute(
        draco::GeometryAttribute::TEX_COORD, 2, draco::DT_FLOAT32);
    const int tangent_id = builder.AddAttribute(
        draco::GeometryAttribute::GENERIC, 4, draco::DT_FLOAT32);
    const int color_id = builder.AddAttribute(
        draco::GeometryAttribute::COLOR, 4, draco::DT_FLOAT32);

    const draco::Vector3f p0(-1.0f, -1.0f, 0.0f);
    const draco::Vector3f p1(1.0f, -1.0f, 0.0f);
    const draco::Vector3f p2(0.0f, 1.0f, 0.0f);
    const draco::Vector3f n0(0.0f, 0.0f, 1.0f);
    const draco::Vector2f uv0(0.0f, 0.0f);
    const draco::Vector2f uv1(1.0f, 0.0f);
    const draco::Vector2f uv2(0.5f, 1.0f);
    const draco::Vector2f uv10(0.25f, 0.25f);
    const draco::Vector2f uv11(0.75f, 0.25f);
    const draco::Vector2f uv12(0.5f, 0.75f);
    const std::array<float, 4> tangent0 = {1.0f, 0.0f, 0.0f, 1.0f};
    const std::array<float, 4> red = {1.0f, 0.0f, 0.0f, 1.0f};
    const std::array<float, 4> green = {0.0f, 1.0f, 0.0f, 1.0f};
    const std::array<float, 4> blue = {0.0f, 0.0f, 1.0f, 1.0f};

    builder.SetAttributeValuesForFace(
        position_id, draco::FaceIndex(0), p0.data(), p1.data(), p2.data());
    builder.SetAttributeValuesForFace(
        normal_id, draco::FaceIndex(0), n0.data(), n0.data(), n0.data());
    builder.SetAttributeValuesForFace(
        texcoord0_id, draco::FaceIndex(0), uv0.data(), uv1.data(), uv2.data());
    builder.SetAttributeValuesForFace(
        texcoord1_id, draco::FaceIndex(0), uv10.data(), uv11.data(), uv12.data());
    builder.SetAttributeValuesForFace(
        tangent_id, draco::FaceIndex(0),
        tangent0.data(), tangent0.data(), tangent0.data());
    builder.SetAttributeValuesForFace(
        color_id, draco::FaceIndex(0), red.data(), green.data(), blue.data());

    builder.SetAttributeUniqueId(position_id, 7);
    builder.SetAttributeUniqueId(normal_id, 8);
    builder.SetAttributeUniqueId(texcoord0_id, 9);
    builder.SetAttributeUniqueId(texcoord1_id, 10);
    builder.SetAttributeUniqueId(tangent_id, 11);
    builder.SetAttributeUniqueId(color_id, 12);

    auto mesh = builder.Finalize();
    REQUIRE(mesh != nullptr);

    draco::Encoder encoder;
    encoder.SetSpeedOptions(10, 10);
    draco::EncoderBuffer buffer;
    const auto status = encoder.EncodeMeshToBuffer(*mesh, &buffer);
    REQUIRE(status.ok());
    REQUIRE(buffer.size() > 0);

    DracoAttributeIds ids;
    ids.position = 7;
    ids.normal = 8;
    ids.texcoord0 = 9;
    ids.texcoord1 = 10;
    ids.tangent = 11;
    ids.color0 = 12;

    const auto decoded = decode_draco(
        reinterpret_cast<const uint8_t*>(buffer.data()),
        buffer.size(),
        ids);

    REQUIRE(draco_decoder_available());
    REQUIRE(decoded.success);
    REQUIRE(decoded.unique_id_attributes_applied);
    REQUIRE(decoded.vertex_count == 3);
    REQUIRE(decoded.face_count == 1);
    REQUIRE(decoded.positions ==
            std::vector<float>{-1.0f, -1.0f, 0.0f,
                                1.0f, -1.0f, 0.0f,
                                0.0f, 1.0f, 0.0f});
    REQUIRE(decoded.normals ==
            std::vector<float>{0.0f, 0.0f, 1.0f,
                                0.0f, 0.0f, 1.0f,
                                0.0f, 0.0f, 1.0f});
    REQUIRE(decoded.tex_coords ==
            std::vector<float>{0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f});
    REQUIRE(decoded.tex_coords1 ==
            std::vector<float>{0.25f, 0.25f, 0.75f, 0.25f, 0.5f, 0.75f});
    REQUIRE(decoded.tangents ==
            std::vector<float>{1.0f, 0.0f, 0.0f, 1.0f,
                                1.0f, 0.0f, 0.0f, 1.0f,
                                1.0f, 0.0f, 0.0f, 1.0f});
    REQUIRE(decoded.colors ==
            std::vector<float>{1.0f, 0.0f, 0.0f, 1.0f,
                                0.0f, 1.0f, 0.0f, 1.0f,
                                0.0f, 0.0f, 1.0f, 1.0f});
    REQUIRE(decoded.indices == std::vector<uint32_t>{0, 1, 2});
}
#endif

TEST_CASE("Renderer3D renders external glTF buffers and images",
          "[render][scene3d][gpu]") {
    const auto dir = std::filesystem::temp_directory_path() /
        "pulp-renderer3d-external-gltf";
    std::filesystem::create_directories(dir);

    const auto bin = make_external_renderable_gltf_bin();
    write_file(dir / "mesh.bin", bin);
    write_file(dir / "checker.png", make_alpha_checker_png());
    const auto gltf_path = dir / "scene.gltf";
    write_file(gltf_path, make_external_renderable_gltf_json(bin.size()));

    auto loaded = pulp::scene::load_gltf_scene(gltf_path);
    INFO(loaded.error);
    REQUIRE(loaded.success);

    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(loaded.scene, config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.primitive_count == 1);
    REQUIRE(result.texture_uploaded);
    REQUIRE(result.texture_decoded);
    REQUIRE_FALSE(result.fallback_texture_used);
    REQUIRE(result.base_color_texture_srgb_applied);
    REQUIRE(result.texture_sampler_applied);
    REQUIRE(result.texture_sampler_clamp_s);
    REQUIRE(result.texture_sampler_clamp_t);
    REQUIRE(result.texture_sampler_linear);
    REQUIRE(result.geometry_normals_applied);
    REQUIRE(result.base_color_factor_applied);
    const auto foreground = find_foreground_region(result.rgba,
                                                   config.width,
                                                   config.height);
    REQUIRE(foreground.pixel_count > 900);
}

TEST_CASE("Renderer3D renders a mesh below a child node", "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(make_child_node_render_scene(), config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.primitive_count == 1);
    REQUIRE(result.fallback_texture_used);
    REQUIRE(result.base_color_texture_srgb_applied);
    REQUIRE_FALSE(result.texture_sampler_applied);
    REQUIRE(result.base_color_factor_applied);
    REQUIRE(result.command_submitted);
    REQUIRE(result.readback_completed);
    auto foreground = find_foreground_region(result.rgba, config.width, config.height);
    REQUIRE(foreground.pixel_count > 1200);
    REQUIRE(result.success);
}

TEST_CASE("Renderer3D applies base-color texture transform metadata",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(make_transformed_uv_render_scene(), config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.primitive_count == 1);
    REQUIRE(result.fallback_texture_used);
    REQUIRE(result.base_color_factor_applied);
    REQUIRE(result.base_color_transform_applied);
    REQUIRE(result.base_color_texcoord1_used);
    REQUIRE(result.command_submitted);
    REQUIRE(result.readback_completed);
    auto foreground = find_foreground_region(result.rgba, config.width, config.height);
    REQUIRE(foreground.pixel_count > 1200);
    REQUIRE(result.success);
}

TEST_CASE("Renderer3D downgrades mipmap sampler state for single-level textures",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(
        make_mipmap_sampler_render_scene(),
        config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.primitive_count == 1);
    REQUIRE(result.fallback_texture_used);
    REQUIRE(result.texture_sampler_applied);
    REQUIRE(result.texture_sampler_linear);
    REQUIRE(result.texture_mipmap_filter_downgraded);
    REQUIRE(result.base_color_texture_srgb_applied);
    const auto foreground = find_foreground_region(result.rgba,
                                                   config.width,
                                                   config.height);
    REQUIRE(foreground.pixel_count > 1200);
}

TEST_CASE("Renderer3D renders multiple same-state SceneData primitives",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(make_multi_primitive_render_scene(),
                                                config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.primitive_count == 2);
    REQUIRE(result.pipeline_cache_entry_count == 1);
    REQUIRE(result.pipeline_cache_hit_count == 1);
    REQUIRE(result.fallback_texture_used);
    REQUIRE(result.base_color_factor_applied);
    const auto foreground = find_foreground_region(result.rgba,
                                                   config.width,
                                                   config.height);
    REQUIRE(foreground.pixel_count > 1400);
}

TEST_CASE("Renderer3D applies per-primitive material uniforms",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(
        make_mixed_material_primitive_render_scene(),
        config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.primitive_count == 2);
    REQUIRE(result.pipeline_cache_entry_count == 1);
    REQUIRE(result.pipeline_cache_hit_count == 1);
    REQUIRE(result.base_color_factor_applied);
    REQUIRE(result.double_sided_material_applied);
    const double red = average_foreground_channel(result.rgba,
                                                  config.width,
                                                  config.height,
                                                  0);
    const double green = average_foreground_channel(result.rgba,
                                                    config.width,
                                                    config.height,
                                                    1);
    const double blue = average_foreground_channel(result.rgba,
                                                   config.width,
                                                   config.height,
                                                   2);
    REQUIRE(red > 45.0);
    REQUIRE(green > 45.0);
    REQUIRE(blue < red * 0.35);
    REQUIRE(blue < green * 0.35);
}

TEST_CASE("Renderer3D applies directional light color",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(
        make_directional_light_render_scene(),
        config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.directional_light_applied);
    REQUIRE(result.base_color_factor_applied);
    const double red = average_foreground_channel(result.rgba,
                                                  config.width,
                                                  config.height,
                                                  0);
    const double green = average_foreground_channel(result.rgba,
                                                    config.width,
                                                    config.height,
                                                    1);
    const double blue = average_foreground_channel(result.rgba,
                                                   config.width,
                                                   config.height,
                                                   2);
    REQUIRE(green > 60.0);
    REQUIRE(red < green * 0.25);
    REQUIRE(blue < green * 0.25);
}

TEST_CASE("Renderer3D applies directional light node transform",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto front_lit = Renderer3D::render_scene_data(
        make_directional_light_direction_render_scene(false),
        config);
    if (!front_lit.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: "
                << front_lit.error);
        return;
    }
    auto back_lit = Renderer3D::render_scene_data(
        make_directional_light_direction_render_scene(true),
        config);

    INFO(front_lit.error);
    INFO(back_lit.error);
    REQUIRE(front_lit.success);
    REQUIRE(back_lit.success);
    REQUIRE(front_lit.directional_light_applied);
    REQUIRE(back_lit.directional_light_applied);
    REQUIRE_FALSE(front_lit.directional_light_transform_applied);
    REQUIRE(back_lit.directional_light_transform_applied);
    REQUIRE_FALSE(back_lit.light_node_transform_deferred);
    REQUIRE(front_lit.geometry_normals_applied);
    REQUIRE(back_lit.geometry_normals_applied);

    const auto front_region = find_foreground_region(front_lit.rgba,
                                                     config.width,
                                                     config.height);
    const auto back_region = find_foreground_region(back_lit.rgba,
                                                    config.width,
                                                    config.height);
    REQUIRE(front_region.pixel_count > 1200);
    REQUIRE(back_region.pixel_count > 1200);
    REQUIRE(average_foreground_luma(front_lit.rgba,
                                    config.width,
                                    config.height) >
            average_foreground_luma(back_lit.rgba,
                                    config.width,
                                    config.height) + 20.0);
}

TEST_CASE("Renderer3D applies point and spot lights and defers range metadata",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto unlit_punctual = Renderer3D::render_scene_data(
        make_deferred_punctual_light_render_scene(false, false),
        config);
    if (!unlit_punctual.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " <<
                unlit_punctual.error);
        return;
    }
    auto baseline = Renderer3D::render_scene_data(
        make_deferred_punctual_light_render_scene(false),
        config);
    auto result = Renderer3D::render_scene_data(
        make_deferred_punctual_light_render_scene(),
        config);

    INFO(unlit_punctual.error);
    INFO(baseline.error);
    INFO(result.error);
    REQUIRE(unlit_punctual.success);
    REQUIRE(baseline.success);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE_FALSE(unlit_punctual.punctual_light_range_applied);
    REQUIRE(unlit_punctual.punctual_light_range_deferred);
    REQUIRE_FALSE(baseline.point_light_applied);
    REQUIRE(baseline.point_light_deferred);
    REQUIRE(baseline.spot_light_applied);
    REQUIRE_FALSE(baseline.spot_light_deferred);
    REQUIRE_FALSE(baseline.spot_light_cone_deferred);
    REQUIRE(baseline.punctual_light_range_applied);
    REQUIRE(baseline.punctual_light_range_deferred);
    REQUIRE_FALSE(result.directional_light_applied);
    REQUIRE(result.point_light_applied);
    REQUIRE_FALSE(result.point_light_deferred);
    REQUIRE(result.spot_light_applied);
    REQUIRE_FALSE(result.spot_light_deferred);
    REQUIRE(result.punctual_light_range_applied);
    REQUIRE_FALSE(result.punctual_light_range_deferred);
    REQUIRE_FALSE(result.spot_light_cone_deferred);
    REQUIRE(result.base_color_factor_applied);
    REQUIRE(result.fallback_texture_used);
    const auto foreground = find_foreground_region(result.rgba,
                                                   config.width,
                                                   config.height);
    REQUIRE(foreground.pixel_count > 1200);
    const double red = average_foreground_channel(result.rgba,
                                                  config.width,
                                                  config.height,
                                                  0);
    const double green = average_foreground_channel(result.rgba,
                                                    config.width,
                                                    config.height,
                                                    1);
    const double blue = average_foreground_channel(result.rgba,
                                                   config.width,
                                                   config.height,
                                                   2);
    const double unlit_blue =
        average_foreground_channel(unlit_punctual.rgba,
                                   config.width,
                                   config.height,
                                   2);
    const double baseline_blue = average_foreground_channel(baseline.rgba,
                                                            config.width,
                                                            config.height,
                                                            2);
    const double baseline_red = average_foreground_channel(baseline.rgba,
                                                           config.width,
                                                           config.height,
                                                           0);
    REQUIRE(baseline_blue > unlit_blue + 10.0);
    REQUIRE(red > baseline_red + 10.0);
    REQUIRE(red > green - 4.0);
    REQUIRE(blue > unlit_blue + 10.0);
}

TEST_CASE("Renderer3D applies point light range attenuation",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto short_range = Renderer3D::render_scene_data(
        make_point_light_range_render_scene(1.5f),
        config);
    if (!short_range.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " <<
                short_range.error);
        return;
    }
    auto long_range = Renderer3D::render_scene_data(
        make_point_light_range_render_scene(12.0f),
        config);

    INFO(short_range.error);
    INFO(long_range.error);
    REQUIRE(short_range.success);
    REQUIRE(long_range.success);
    REQUIRE(short_range.point_light_applied);
    REQUIRE(long_range.point_light_applied);
    REQUIRE(short_range.punctual_light_range_applied);
    REQUIRE(long_range.punctual_light_range_applied);
    REQUIRE_FALSE(short_range.punctual_light_range_deferred);
    REQUIRE_FALSE(long_range.punctual_light_range_deferred);

    const double short_red = average_foreground_channel(short_range.rgba,
                                                        config.width,
                                                        config.height,
                                                        0);
    const double long_red = average_foreground_channel(long_range.rgba,
                                                       config.width,
                                                       config.height,
                                                       0);
    REQUIRE(long_red > short_red + 18.0);
}

TEST_CASE("Renderer3D applies preserved perspective camera yfov",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto no_camera = Renderer3D::render_scene_data(
        make_perspective_camera_render_scene(false),
        config);
    if (!no_camera.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << no_camera.error);
        return;
    }
    auto with_camera = Renderer3D::render_scene_data(
        make_perspective_camera_render_scene(true),
        config);

    INFO(no_camera.error);
    INFO(with_camera.error);
    REQUIRE(no_camera.success);
    REQUIRE(with_camera.success);
    REQUIRE_FALSE(no_camera.perspective_camera_applied);
    REQUIRE(with_camera.perspective_camera_applied);
    REQUIRE(with_camera.base_color_factor_applied);

    const auto no_camera_region = find_foreground_region(no_camera.rgba,
                                                         config.width,
                                                         config.height);
    const auto with_camera_region = find_foreground_region(with_camera.rgba,
                                                           config.width,
                                                           config.height);
    REQUIRE(no_camera_region.pixel_count > 500);
    REQUIRE(with_camera_region.pixel_count >
            no_camera_region.pixel_count + 600);
}

TEST_CASE("Renderer3D reports deferred camera and light node transforms",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(
        make_transformed_camera_light_render_scene(),
        config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.perspective_camera_applied);
    REQUIRE(result.directional_light_applied);
    REQUIRE(result.directional_light_transform_applied);
    REQUIRE(result.camera_node_translation_applied);
    REQUIRE(result.camera_node_rotation_applied);
    REQUIRE_FALSE(result.camera_node_transform_deferred);
    REQUIRE_FALSE(result.light_node_transform_deferred);
    REQUIRE(result.base_color_factor_applied);
    REQUIRE(result.command_submitted);
    REQUIRE(result.readback_completed);
    REQUIRE(result.rgba.size() ==
            static_cast<size_t>(config.width) * config.height * 4u);
}

TEST_CASE("Renderer3D reports non-rigid light node transforms",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto scene = make_transformed_camera_light_render_scene();
    for (auto& node : scene.nodes) {
        if (node.light == 0) {
            node.scale[0] = 2.0f;
        }
    }

    auto result = Renderer3D::render_scene_data(scene, config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.directional_light_applied);
    REQUIRE(result.directional_light_transform_applied);
    REQUIRE(result.light_node_transform_deferred);
}

TEST_CASE("Renderer3D applies camera node rotation as view basis",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto translated = Renderer3D::render_scene_data(
        make_translated_camera_render_scene(-0.25f),
        config);
    if (!translated.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: "
                << translated.error);
        return;
    }
    auto rotated = Renderer3D::render_scene_data(
        make_rotated_camera_render_scene(0.382683f, 0.92388f),
        config);

    INFO(translated.error);
    INFO(rotated.error);
    REQUIRE(translated.success);
    REQUIRE(rotated.success);
    REQUIRE(translated.perspective_camera_applied);
    REQUIRE(rotated.perspective_camera_applied);
    REQUIRE(translated.camera_node_translation_applied);
    REQUIRE(rotated.camera_node_translation_applied);
    REQUIRE_FALSE(translated.camera_node_rotation_applied);
    REQUIRE(rotated.camera_node_rotation_applied);
    REQUIRE_FALSE(rotated.camera_node_transform_deferred);

    const auto translated_region = find_foreground_region(translated.rgba,
                                                          config.width,
                                                          config.height);
    const auto rotated_region = find_foreground_region(rotated.rgba,
                                                       config.width,
                                                       config.height);
    REQUIRE(translated_region.pixel_count > 1200);
    REQUIRE(rotated_region.pixel_count > 1200);
    REQUIRE(std::abs(foreground_center_x(rotated_region) -
                     foreground_center_x(translated_region)) > 2.0);
}

TEST_CASE("Renderer3D applies rigid camera matrix transform as view basis",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto translated = Renderer3D::render_scene_data(
        make_matrix_camera_render_scene(false),
        config);
    if (!translated.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: "
                << translated.error);
        return;
    }
    auto rotated = Renderer3D::render_scene_data(
        make_matrix_camera_render_scene(true),
        config);

    INFO(translated.error);
    INFO(rotated.error);
    REQUIRE(translated.success);
    REQUIRE(rotated.success);
    REQUIRE(translated.perspective_camera_applied);
    REQUIRE(rotated.perspective_camera_applied);
    REQUIRE(translated.camera_node_translation_applied);
    REQUIRE(rotated.camera_node_translation_applied);
    REQUIRE_FALSE(translated.camera_node_rotation_applied);
    REQUIRE(rotated.camera_node_rotation_applied);
    REQUIRE_FALSE(translated.camera_node_transform_deferred);
    REQUIRE_FALSE(rotated.camera_node_transform_deferred);

    const auto translated_region = find_foreground_region(translated.rgba,
                                                          config.width,
                                                          config.height);
    const auto rotated_region = find_foreground_region(rotated.rgba,
                                                       config.width,
                                                       config.height);
    REQUIRE(translated_region.pixel_count > 1200);
    REQUIRE(rotated_region.pixel_count > 1200);
    REQUIRE(std::abs(foreground_center_x(rotated_region) -
                     foreground_center_x(translated_region)) > 2.0);
}

TEST_CASE("Renderer3D applies camera node translation as a view offset",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto centered = Renderer3D::render_scene_data(
        make_perspective_camera_render_scene(true),
        config);
    if (!centered.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: "
                << centered.error);
        return;
    }
    auto shifted = Renderer3D::render_scene_data(
        make_translated_camera_render_scene(-0.25f),
        config);

    INFO(centered.error);
    INFO(shifted.error);
    REQUIRE(centered.success);
    REQUIRE(shifted.success);
    REQUIRE(centered.perspective_camera_applied);
    REQUIRE(shifted.perspective_camera_applied);
    REQUIRE_FALSE(centered.camera_node_translation_applied);
    REQUIRE(shifted.camera_node_translation_applied);
    REQUIRE_FALSE(shifted.camera_node_transform_deferred);

    const auto centered_region = find_foreground_region(centered.rgba,
                                                        config.width,
                                                        config.height);
    const auto shifted_region = find_foreground_region(shifted.rgba,
                                                       config.width,
                                                       config.height);
    REQUIRE(centered_region.pixel_count > 1200);
    REQUIRE(shifted_region.pixel_count > 1200);
    REQUIRE(shifted_region.min_x > centered_region.min_x + 8);
}

TEST_CASE("Renderer3D applies preserved camera projection metadata",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(
        make_camera_metadata_render_scene(),
        config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.perspective_camera_applied);
    REQUIRE(result.camera_aspect_ratio_applied);
    REQUIRE_FALSE(result.camera_aspect_ratio_deferred);
    REQUIRE(result.camera_depth_range_applied);
    REQUIRE_FALSE(result.camera_depth_range_deferred);
    REQUIRE(result.base_color_factor_applied);
    const auto foreground = find_foreground_region(result.rgba,
                                                   config.width,
                                                   config.height);
    REQUIRE(foreground.pixel_count > 1200);
}

TEST_CASE("Renderer3D applies preserved camera aspect ratio",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto square = Renderer3D::render_scene_data(
        make_camera_aspect_ratio_render_scene(1.0f),
        config);
    if (!square.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << square.error);
        return;
    }
    auto wide = Renderer3D::render_scene_data(
        make_camera_aspect_ratio_render_scene(2.0f),
        config);

    INFO(square.error);
    INFO(wide.error);
    REQUIRE(square.success);
    REQUIRE(wide.success);
    REQUIRE(square.perspective_camera_applied);
    REQUIRE(wide.perspective_camera_applied);
    REQUIRE(square.camera_aspect_ratio_applied);
    REQUIRE(wide.camera_aspect_ratio_applied);
    REQUIRE_FALSE(square.camera_aspect_ratio_deferred);
    REQUIRE_FALSE(wide.camera_aspect_ratio_deferred);

    const auto square_region = find_foreground_region(square.rgba,
                                                      config.width,
                                                      config.height);
    const auto wide_region = find_foreground_region(wide.rgba,
                                                    config.width,
                                                    config.height);
    REQUIRE(square_region.pixel_count > 1200);
    REQUIRE(wide_region.pixel_count > 900);
    REQUIRE(foreground_width(square_region) >
            foreground_width(wide_region) + 12u);
}

TEST_CASE("Renderer3D applies preserved orthographic camera ymag",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto wide = Renderer3D::render_scene_data(
        make_orthographic_camera_render_scene(3.0f),
        config);
    if (!wide.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << wide.error);
        return;
    }
    auto narrow = Renderer3D::render_scene_data(
        make_orthographic_camera_render_scene(1.2f),
        config);

    INFO(wide.error);
    INFO(narrow.error);
    REQUIRE(wide.success);
    REQUIRE(narrow.success);
    REQUIRE_FALSE(wide.perspective_camera_applied);
    REQUIRE_FALSE(narrow.perspective_camera_applied);
    REQUIRE(wide.orthographic_camera_applied);
    REQUIRE(narrow.orthographic_camera_applied);
    REQUIRE(wide.base_color_factor_applied);
    REQUIRE(narrow.base_color_factor_applied);

    const auto wide_region = find_foreground_region(wide.rgba,
                                                    config.width,
                                                    config.height);
    const auto narrow_region = find_foreground_region(narrow.rgba,
                                                      config.width,
                                                      config.height);
    REQUIRE(narrow_region.pixel_count > wide_region.pixel_count + 400);
}

TEST_CASE("Renderer3D applies transform animation initial pose and defers playback",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto static_scene = make_transform_animation_render_scene(0.0f);
    static_scene.animations.clear();
    auto static_result = Renderer3D::render_scene_data(static_scene, config);
    if (!static_result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: "
                << static_result.error);
        return;
    }

    auto result = Renderer3D::render_scene_data(
        make_transform_animation_render_scene(),
        config);

    INFO(static_result.error);
    INFO(result.error);
    REQUIRE(static_result.success);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.transform_animation_initial_pose_applied);
    REQUIRE(result.transform_animation_deferred);
    REQUIRE(result.base_color_factor_applied);
    REQUIRE(result.fallback_texture_used);
    REQUIRE_FALSE(static_result.transform_animation_initial_pose_applied);
    REQUIRE_FALSE(static_result.transform_animation_deferred);
    const auto static_foreground =
        find_foreground_region(static_result.rgba, config.width, config.height);
    const auto foreground = find_foreground_region(result.rgba,
                                                   config.width,
                                                   config.height);
    REQUIRE(static_foreground.pixel_count > 1200);
    REQUIRE(foreground.pixel_count > 1200);
    const HeadlessSurface::Rgba static_rgba{
        static_result.rgba,
        static_result.width,
        static_result.height,
    };
    const HeadlessSurface::Rgba animated_rgba{
        result.rgba,
        result.width,
        result.height,
    };
    REQUIRE(HeadlessSurface::rgba_fingerprint(animated_rgba) !=
            HeadlessSurface::rgba_fingerprint(static_rgba));
}

TEST_CASE("Renderer3D applies rotation and scale animation initial poses",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto assert_initial_pose_changes_render =
        [&](pulp::scene::SceneData scene) {
            auto static_scene = scene;
            static_scene.animations.clear();
            auto static_result =
                Renderer3D::render_scene_data(static_scene, config);
            if (!static_result.gpu_available) {
                SUCCEED("Dawn/WebGPU unavailable in this environment: "
                        << static_result.error);
                return;
            }

            auto result = Renderer3D::render_scene_data(scene, config);

            INFO(static_result.error);
            INFO(result.error);
            REQUIRE(static_result.success);
            REQUIRE(result.success);
            REQUIRE(result.transform_animation_initial_pose_applied);
            REQUIRE(result.transform_animation_deferred);
            REQUIRE_FALSE(static_result.transform_animation_initial_pose_applied);
            REQUIRE_FALSE(static_result.transform_animation_deferred);

            const auto static_foreground =
                find_foreground_region(static_result.rgba,
                                       config.width,
                                       config.height);
            const auto foreground =
                find_foreground_region(result.rgba, config.width, config.height);
            REQUIRE(static_foreground.pixel_count > 1200);
            REQUIRE(foreground.pixel_count > 1200);

            const HeadlessSurface::Rgba static_rgba{
                static_result.rgba,
                static_result.width,
                static_result.height,
            };
            const HeadlessSurface::Rgba animated_rgba{
                result.rgba,
                result.width,
                result.height,
            };
            REQUIRE(HeadlessSurface::rgba_fingerprint(animated_rgba) !=
                    HeadlessSurface::rgba_fingerprint(static_rgba));
        };

    SECTION("cubic spline rotation") {
        assert_initial_pose_changes_render(make_rotation_animation_render_scene());
    }

    SECTION("scale") {
        assert_initial_pose_changes_render(make_scale_animation_render_scene());
    }
}

TEST_CASE("Renderer3D reports deferred unsupported glTF feature records",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(
        make_unsupported_feature_deferred_render_scene(),
        config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.skinning_deferred);
    REQUIRE(result.morph_target_deferred);
    REQUIRE(result.gpu_instancing_deferred);
    REQUIRE(result.perspective_camera_applied);
    REQUIRE(result.base_color_factor_applied);
    const auto foreground = find_foreground_region(result.rgba,
                                                   config.width,
                                                   config.height);
    REQUIRE(foreground.pixel_count > 1200);
}

TEST_CASE("Renderer3D reports deferred advanced material extensions",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(
        make_advanced_material_extension_render_scene(),
        config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.advanced_material_extension_deferred);
    REQUIRE(result.base_color_factor_applied);
    REQUIRE(result.double_sided_material_applied);
    REQUIRE(result.fallback_texture_used);
    const auto foreground = find_foreground_region(result.rgba,
                                                   config.width,
                                                   config.height);
    REQUIRE(foreground.pixel_count > 1200);
}

TEST_CASE("Renderer3D applies unlit material shading",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto lit = Renderer3D::render_scene_data(make_unlit_render_scene(false), config);
    if (!lit.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << lit.error);
        return;
    }
    auto unlit = Renderer3D::render_scene_data(make_unlit_render_scene(true), config);

    INFO(lit.error);
    INFO(unlit.error);
    REQUIRE(lit.success);
    REQUIRE(unlit.success);
    REQUIRE_FALSE(lit.unlit_material_applied);
    REQUIRE(unlit.unlit_material_applied);
    REQUIRE(lit.fallback_texture_used);
    REQUIRE(unlit.fallback_texture_used);
    REQUIRE(average_foreground_luma(unlit.rgba, config.width, config.height) >
            average_foreground_luma(lit.rgba, config.width, config.height) + 10.0);
}

TEST_CASE("Renderer3D applies alpha-mask material cutoff",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto opaque = Renderer3D::render_scene_data(make_alpha_mask_render_scene(false),
                                                config);
    if (!opaque.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << opaque.error);
        return;
    }
    auto masked = Renderer3D::render_scene_data(make_alpha_mask_render_scene(true),
                                                config);

    INFO(opaque.error);
    INFO(masked.error);
    REQUIRE(opaque.success);
    REQUIRE(masked.success);
    REQUIRE(opaque.texture_decoded);
    REQUIRE(masked.texture_decoded);
    REQUIRE_FALSE(opaque.alpha_mask_applied);
    REQUIRE(masked.alpha_mask_applied);
    const auto opaque_foreground = find_foreground_region(opaque.rgba,
                                                          config.width,
                                                          config.height);
    const auto masked_foreground = find_foreground_region(masked.rgba,
                                                          config.width,
                                                          config.height);
    REQUIRE(masked_foreground.pixel_count < opaque_foreground.pixel_count);
}

TEST_CASE("Renderer3D applies vertex colors",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(make_vertex_color_render_scene(),
                                                config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.vertex_color_applied);
    REQUIRE(result.fallback_texture_used);
    const double red = average_foreground_channel(result.rgba, config.width, config.height, 0);
    const double green = average_foreground_channel(result.rgba, config.width, config.height, 1);
    const double blue = average_foreground_channel(result.rgba, config.width, config.height, 2);
    REQUIRE(red > 40.0);
    REQUIRE(green < red * 0.25);
    REQUIRE(blue < red * 0.25);
}

TEST_CASE("Renderer3D applies geometry normals to lighting",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto front = Renderer3D::render_scene_data(
        make_geometry_normal_render_scene(true),
        config);
    if (!front.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << front.error);
        return;
    }
    auto back = Renderer3D::render_scene_data(
        make_geometry_normal_render_scene(false),
        config);

    INFO(front.error);
    INFO(back.error);
    REQUIRE(front.success);
    REQUIRE(back.success);
    REQUIRE(front.geometry_normals_applied);
    REQUIRE(back.geometry_normals_applied);
    REQUIRE(front.fallback_texture_used);
    REQUIRE(back.fallback_texture_used);
    const double front_luma = average_image_luma(front.rgba,
                                                 config.width,
                                                 config.height);
    const double back_luma = average_image_luma(back.rgba,
                                                config.width,
                                                config.height);
    REQUIRE(front_luma > back_luma + 5.0);
}

TEST_CASE("Renderer3D reports deferred normal-map inputs",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(
        make_normal_texture_deferred_render_scene(),
        config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.primitive_count == 1);
    REQUIRE(result.geometry_normals_applied);
    REQUIRE(result.tangent_attributes_available);
    REQUIRE(result.normal_texture_deferred);
    REQUIRE(result.normal_scale_deferred);
    REQUIRE(result.fallback_texture_used);
    REQUIRE_FALSE(result.texture_decoded);
    REQUIRE(result.base_color_factor_applied);
    const auto foreground = find_foreground_region(result.rgba,
                                                   config.width,
                                                   config.height);
    REQUIRE(foreground.pixel_count > 1200);
}

TEST_CASE("Renderer3D samples normal material texture",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto plain = Renderer3D::render_scene_data(
        make_geometry_normal_render_scene(true),
        config);
    if (!plain.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << plain.error);
        return;
    }
    auto normal_mapped = Renderer3D::render_scene_data(
        make_normal_texture_render_scene(),
        config);

    INFO(plain.error);
    INFO(normal_mapped.error);
    REQUIRE(plain.success);
    REQUIRE(normal_mapped.success);
    REQUIRE_FALSE(plain.normal_texture_applied);
    REQUIRE(normal_mapped.normal_texture_applied);
    REQUIRE_FALSE(normal_mapped.normal_scale_applied);
    REQUIRE_FALSE(normal_mapped.normal_texture_deferred);
    REQUIRE_FALSE(normal_mapped.normal_scale_deferred);
    REQUIRE(normal_mapped.texture_decoded);
    REQUIRE(normal_mapped.fallback_texture_used);
    const double plain_luma = average_image_luma(plain.rgba,
                                                 config.width,
                                                 config.height);
    const double normal_luma = average_image_luma(normal_mapped.rgba,
                                                  config.width,
                                                  config.height);
    REQUIRE(normal_luma < plain_luma - 5.0);
}

TEST_CASE("Renderer3D applies normal texture scale",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto default_scale = Renderer3D::render_scene_data(
        make_normal_texture_render_scene(1.0f, true),
        config);
    if (!default_scale.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: "
                << default_scale.error);
        return;
    }
    auto boosted_scale = Renderer3D::render_scene_data(
        make_normal_texture_render_scene(2.0f, true),
        config);

    INFO(default_scale.error);
    INFO(boosted_scale.error);
    REQUIRE(default_scale.success);
    REQUIRE(boosted_scale.success);
    REQUIRE(default_scale.normal_texture_applied);
    REQUIRE(boosted_scale.normal_texture_applied);
    REQUIRE_FALSE(default_scale.normal_scale_applied);
    REQUIRE(boosted_scale.normal_scale_applied);
    REQUIRE_FALSE(boosted_scale.normal_texture_deferred);
    REQUIRE_FALSE(boosted_scale.normal_scale_deferred);
    const HeadlessSurface::Rgba default_rgba{
        default_scale.rgba,
        default_scale.width,
        default_scale.height,
    };
    const HeadlessSurface::Rgba boosted_rgba{
        boosted_scale.rgba,
        boosted_scale.width,
        boosted_scale.height,
    };
    REQUIRE(HeadlessSurface::rgba_fingerprint(boosted_rgba) !=
            HeadlessSurface::rgba_fingerprint(default_rgba));
}

TEST_CASE("Renderer3D derives tangents for normal material texture",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto plain = Renderer3D::render_scene_data(
        make_geometry_normal_render_scene(true),
        config);
    if (!plain.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << plain.error);
        return;
    }
    auto normal_mapped = Renderer3D::render_scene_data(
        make_normal_texture_derived_tangent_render_scene(),
        config);

    INFO(plain.error);
    INFO(normal_mapped.error);
    REQUIRE(plain.success);
    REQUIRE(normal_mapped.success);
    REQUIRE_FALSE(plain.normal_texture_applied);
    REQUIRE(normal_mapped.tangent_attributes_available);
    REQUIRE(normal_mapped.tangent_attributes_derived);
    REQUIRE(normal_mapped.normal_texture_applied);
    REQUIRE(normal_mapped.normal_scale_applied);
    REQUIRE_FALSE(normal_mapped.normal_texture_deferred);
    REQUIRE_FALSE(normal_mapped.normal_scale_deferred);
    REQUIRE(normal_mapped.texture_decoded);
    REQUIRE(normal_mapped.fallback_texture_used);
    const double plain_luma = average_image_luma(plain.rgba,
                                                 config.width,
                                                 config.height);
    const double normal_luma = average_image_luma(normal_mapped.rgba,
                                                  config.width,
                                                  config.height);
    REQUIRE(normal_luma < plain_luma - 5.0);
}

TEST_CASE("Renderer3D reports remaining deferred PBR texture slots",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(
        make_pbr_texture_slots_deferred_render_scene(),
        config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.primitive_count == 1);
    REQUIRE(result.geometry_normals_applied);
    REQUIRE(result.metallic_roughness_texture_applied);
    REQUIRE_FALSE(result.metallic_roughness_texture_deferred);
    REQUIRE_FALSE(result.tangent_attributes_derived);
    REQUIRE(result.normal_texture_deferred);
    REQUIRE(result.normal_scale_deferred);
    REQUIRE(result.occlusion_texture_applied);
    REQUIRE_FALSE(result.occlusion_texture_deferred);
    REQUIRE_FALSE(result.occlusion_strength_deferred);
    REQUIRE(result.emissive_texture_applied);
    REQUIRE_FALSE(result.emissive_texture_deferred);
    REQUIRE(result.non_base_color_texture_transform_applied);
    REQUIRE(result.non_base_color_texcoord1_used);
    REQUIRE_FALSE(result.non_base_color_texture_transform_deferred);
    REQUIRE_FALSE(result.non_base_color_texcoord1_deferred);
    REQUIRE(result.emissive_factor_applied);
    REQUIRE(result.fallback_texture_used);
    REQUIRE(result.texture_decoded);
    const auto foreground = find_foreground_region(result.rgba,
                                                   config.width,
                                                   config.height);
    REQUIRE(foreground.pixel_count > 1200);
}

TEST_CASE("Renderer3D deferred flags stay aligned with sidecar native gaps",
          "[render][scene3d][gpu][bake]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto unsupported_scene = make_unsupported_feature_deferred_render_scene();
    auto unsupported_render = Renderer3D::render_scene_data(unsupported_scene,
                                                            config);
    if (!unsupported_render.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: "
                << unsupported_render.error);
        return;
    }
    auto unsupported_report = pulp::scene::analyze_bake_preflight(
        pulp::scene::build_sidecar_from_scene(unsupported_scene));

    REQUIRE(unsupported_render.success);
    REQUIRE(unsupported_render.skinning_deferred);
    REQUIRE(unsupported_render.morph_target_deferred);
    REQUIRE(unsupported_render.gpu_instancing_deferred);
    REQUIRE(unsupported_report.native_runtime_has_gaps);
    REQUIRE(has_native_runtime_gap(unsupported_report, "Skinning"));
    REQUIRE(has_native_runtime_gap(unsupported_report, "MorphTargets"));
    REQUIRE(has_native_runtime_gap(unsupported_report, "GpuInstancing"));
    REQUIRE(std::string(pulp::scene::bake_preflight_readiness(
                unsupported_report)) == "native_gaps");

    auto animated_scene = make_transform_animation_render_scene();
    auto animated_render = Renderer3D::render_scene_data(animated_scene,
                                                         config);
    auto animated_report = pulp::scene::analyze_bake_preflight(
        pulp::scene::build_sidecar_from_scene(animated_scene));

    REQUIRE(animated_render.success);
    REQUIRE(animated_render.transform_animation_initial_pose_applied);
    REQUIRE(animated_render.transform_animation_deferred);
    REQUIRE(animated_report.native_runtime_has_gaps);
    REQUIRE(has_native_runtime_gap(animated_report, "TransformAnimation"));
    REQUIRE(std::string(pulp::scene::bake_preflight_readiness(
                animated_report)) == "native_gaps");

    auto material_scene = make_pbr_texture_slots_deferred_render_scene();
    auto material_render = Renderer3D::render_scene_data(material_scene,
                                                         config);
    auto material_report = pulp::scene::analyze_bake_preflight(
        pulp::scene::build_sidecar_from_scene(material_scene));

    REQUIRE(material_render.success);
    REQUIRE(material_render.normal_texture_deferred);
    REQUIRE(material_render.normal_scale_deferred);
    REQUIRE_FALSE(material_render.occlusion_strength_deferred);
    REQUIRE(material_render.non_base_color_texture_transform_applied);
    REQUIRE(material_render.non_base_color_texcoord1_used);
    REQUIRE_FALSE(material_render.non_base_color_texture_transform_deferred);
    REQUIRE_FALSE(material_render.non_base_color_texcoord1_deferred);
    REQUIRE(material_report.native_runtime_has_gaps);
    REQUIRE(has_native_runtime_gap(material_report,
                                   "MaterialTexture:normalTangents"));
    REQUIRE(has_native_runtime_gap(material_report,
                                   "MaterialTexture:normalScale"));
    REQUIRE_FALSE(has_native_runtime_gap(
        material_report,
        "MaterialTextureTransform:nonBaseColor"));
    REQUIRE_FALSE(has_native_runtime_gap(material_report,
                                         "MaterialTexcoord:nonBaseColor"));
    REQUIRE_FALSE(has_native_runtime_gap(
        material_report,
        "MaterialTexture:occlusionStrength"));
    REQUIRE(std::string(pulp::scene::bake_preflight_readiness(
                material_report)) == "native_gaps");
}

TEST_CASE("Renderer3D applies double-sided material culling",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto single_sided = Renderer3D::render_scene_data(
        make_back_facing_render_scene(false),
        config);
    if (!single_sided.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << single_sided.error);
        return;
    }
    auto double_sided = Renderer3D::render_scene_data(
        make_back_facing_render_scene(true),
        config);

    INFO(single_sided.error);
    INFO(double_sided.error);
    REQUIRE_FALSE(single_sided.double_sided_material_applied);
    REQUIRE(double_sided.double_sided_material_applied);
    const auto single_foreground = find_foreground_region(single_sided.rgba,
                                                          config.width,
                                                          config.height);
    const auto double_foreground = find_foreground_region(double_sided.rgba,
                                                          config.width,
                                                          config.height);
    REQUIRE(single_foreground.pixel_count < 100);
    REQUIRE(double_foreground.pixel_count > 1200);
    REQUIRE(double_sided.success);
}

TEST_CASE("Renderer3D applies alpha-blend material state",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto opaque = Renderer3D::render_scene_data(make_alpha_blend_render_scene(false),
                                                config);
    if (!opaque.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << opaque.error);
        return;
    }
    auto blended = Renderer3D::render_scene_data(make_alpha_blend_render_scene(true),
                                                 config);

    INFO(opaque.error);
    INFO(blended.error);
    REQUIRE(opaque.success);
    REQUIRE(blended.success);
    REQUIRE_FALSE(opaque.alpha_blend_applied);
    REQUIRE(blended.alpha_blend_applied);
    const double opaque_luma = average_foreground_luma(opaque.rgba,
                                                       config.width,
                                                       config.height);
    const double blended_luma = average_foreground_luma(blended.rgba,
                                                        config.width,
                                                        config.height);
    REQUIRE(blended_luma < opaque_luma * 0.65);
    REQUIRE(blended_luma > opaque_luma * 0.20);
}

TEST_CASE("Renderer3D draws opaque primitives before alpha-blend primitives",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(
        make_alpha_blend_over_opaque_render_scene(),
        config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.primitive_count == 2);
    REQUIRE(result.alpha_blend_applied);
    REQUIRE(result.alpha_blend_depth_write_disabled);
    REQUIRE(result.pipeline_cache_entry_count == 2);
    REQUIRE(result.pipeline_cache_hit_count == 0);

    const double red = average_foreground_channel(result.rgba,
                                                  config.width,
                                                  config.height,
                                                  0);
    const double green = average_foreground_channel(result.rgba,
                                                    config.width,
                                                    config.height,
                                                    1);
    const double blue = average_foreground_channel(result.rgba,
                                                   config.width,
                                                   config.height,
                                                   2);
    REQUIRE(red > 45.0);
    REQUIRE(green > 45.0);
    REQUIRE(blue < red * 0.35);
    REQUIRE(blue < green * 0.35);
}

TEST_CASE("Renderer3D sorts alpha-blend primitives back to front",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(
        make_alpha_blend_sorted_layers_render_scene(),
        config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.primitive_count == 2);
    REQUIRE(result.alpha_blend_applied);
    REQUIRE(result.alpha_blend_depth_write_disabled);
    REQUIRE(result.alpha_blend_sorted);
    REQUIRE(result.pipeline_cache_entry_count == 1);
    REQUIRE(result.pipeline_cache_hit_count == 1);

    const double red = average_foreground_channel(result.rgba,
                                                  config.width,
                                                  config.height,
                                                  0);
    const double green = average_foreground_channel(result.rgba,
                                                    config.width,
                                                    config.height,
                                                    1);
    const double blue = average_foreground_channel(result.rgba,
                                                   config.width,
                                                   config.height,
                                                   2);
    REQUIRE(red > blue * 1.20);
    REQUIRE(green < red * 0.35);
    REQUIRE(green < blue * 0.60);
}

TEST_CASE("Renderer3D applies emissive material factor",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto plain = Renderer3D::render_scene_data(make_emissive_render_scene(false),
                                               config);
    if (!plain.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << plain.error);
        return;
    }
    auto emissive = Renderer3D::render_scene_data(make_emissive_render_scene(true),
                                                  config);

    INFO(plain.error);
    INFO(emissive.error);
    REQUIRE(plain.success);
    REQUIRE(emissive.success);
    REQUIRE_FALSE(plain.emissive_factor_applied);
    REQUIRE(emissive.emissive_factor_applied);
    REQUIRE_FALSE(emissive.emissive_strength_applied);
    const double plain_blue = average_foreground_channel(plain.rgba,
                                                         config.width,
                                                         config.height,
                                                         2);
    const double emissive_blue = average_foreground_channel(emissive.rgba,
                                                            config.width,
                                                            config.height,
                                                            2);
    REQUIRE(emissive_blue > plain_blue + 80.0);
}

TEST_CASE("Renderer3D applies emissive material strength",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto baseline = Renderer3D::render_scene_data(
        make_emissive_render_scene(true, 1.0f),
        config);
    if (!baseline.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: "
                << baseline.error);
        return;
    }
    auto boosted = Renderer3D::render_scene_data(
        make_emissive_render_scene(true, 1.8f),
        config);

    INFO(baseline.error);
    INFO(boosted.error);
    REQUIRE(baseline.success);
    REQUIRE(boosted.success);
    REQUIRE(baseline.emissive_factor_applied);
    REQUIRE(boosted.emissive_factor_applied);
    REQUIRE_FALSE(baseline.emissive_strength_applied);
    REQUIRE(boosted.emissive_strength_applied);
    const double baseline_blue = average_foreground_channel(baseline.rgba,
                                                            config.width,
                                                            config.height,
                                                            2);
    const double boosted_blue = average_foreground_channel(boosted.rgba,
                                                           config.width,
                                                           config.height,
                                                           2);
    REQUIRE(boosted_blue > baseline_blue + 45.0);
}

TEST_CASE("Renderer3D samples emissive material texture",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(
        make_emissive_texture_render_scene(),
        config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.emissive_factor_applied);
    REQUIRE(result.emissive_texture_applied);
    REQUIRE_FALSE(result.emissive_texture_deferred);
    REQUIRE(result.texture_decoded);
    REQUIRE(result.fallback_texture_used);
    const double red = average_foreground_channel(result.rgba,
                                                  config.width,
                                                  config.height,
                                                  0);
    const double green = average_foreground_channel(result.rgba,
                                                    config.width,
                                                    config.height,
                                                    1);
    const double blue = average_foreground_channel(result.rgba,
                                                   config.width,
                                                   config.height,
                                                   2);
    REQUIRE(blue > red + 80.0);
    REQUIRE(blue > green + 80.0);
}

TEST_CASE("Renderer3D applies emissive texture transform on TEXCOORD_1",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto result = Renderer3D::render_scene_data(
        make_emissive_texture_texcoord1_transform_render_scene(),
        config);
    if (!result.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << result.error);
        return;
    }

    INFO(result.error);
    REQUIRE(result.success);
    REQUIRE(result.scene_data_consumed);
    REQUIRE(result.emissive_factor_applied);
    REQUIRE(result.emissive_texture_applied);
    REQUIRE_FALSE(result.emissive_texture_deferred);
    REQUIRE(result.non_base_color_texture_transform_applied);
    REQUIRE(result.non_base_color_texcoord1_used);
    REQUIRE_FALSE(result.non_base_color_texture_transform_deferred);
    REQUIRE_FALSE(result.non_base_color_texcoord1_deferred);
    REQUIRE(result.texture_decoded);
    REQUIRE(result.fallback_texture_used);
    const double red = average_foreground_channel(result.rgba,
                                                  config.width,
                                                  config.height,
                                                  0);
    const double green = average_foreground_channel(result.rgba,
                                                    config.width,
                                                    config.height,
                                                    1);
    const double blue = average_foreground_channel(result.rgba,
                                                   config.width,
                                                   config.height,
                                                   2);
    REQUIRE(blue > red + 120.0);
    REQUIRE(blue > green + 120.0);
}

TEST_CASE("Renderer3D applies metallic and roughness material factors",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto smooth_dielectric = Renderer3D::render_scene_data(
        make_metallic_roughness_factor_render_scene(0.0f, 0.1f),
        config);
    if (!smooth_dielectric.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " <<
                smooth_dielectric.error);
        return;
    }
    auto rough_metal = Renderer3D::render_scene_data(
        make_metallic_roughness_factor_render_scene(1.0f, 1.0f),
        config);

    INFO(smooth_dielectric.error);
    INFO(rough_metal.error);
    REQUIRE(smooth_dielectric.success);
    REQUIRE(rough_metal.success);
    REQUIRE(smooth_dielectric.metallic_roughness_factor_applied);
    REQUIRE(rough_metal.metallic_roughness_factor_applied);
    REQUIRE(smooth_dielectric.fallback_texture_used);
    REQUIRE(rough_metal.fallback_texture_used);
    const double smooth_luma = average_image_luma(smooth_dielectric.rgba,
                                                  config.width,
                                                  config.height);
    const double rough_luma = average_image_luma(rough_metal.rgba,
                                                 config.width,
                                                 config.height);
    REQUIRE(smooth_luma > rough_luma + 10.0);
}

TEST_CASE("Renderer3D samples metallic-roughness material texture",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto factor_only = Renderer3D::render_scene_data(
        make_metallic_roughness_factor_render_scene(1.0f, 1.0f),
        config);
    if (!factor_only.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " <<
                factor_only.error);
        return;
    }
    auto textured = Renderer3D::render_scene_data(
        make_metallic_roughness_texture_render_scene(),
        config);

    INFO(factor_only.error);
    INFO(textured.error);
    REQUIRE(factor_only.success);
    REQUIRE(textured.success);
    REQUIRE_FALSE(factor_only.metallic_roughness_texture_applied);
    REQUIRE(textured.metallic_roughness_texture_applied);
    REQUIRE_FALSE(textured.metallic_roughness_texture_deferred);
    REQUIRE(textured.texture_decoded);
    REQUIRE(textured.fallback_texture_used);
    const double factor_luma = average_image_luma(factor_only.rgba,
                                                  config.width,
                                                  config.height);
    const double textured_luma = average_image_luma(textured.rgba,
                                                    config.width,
                                                    config.height);
    REQUIRE(textured_luma > factor_luma + 10.0);
}

TEST_CASE("Renderer3D applies metallic-roughness texture transform on TEXCOORD_1",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto untransformed = Renderer3D::render_scene_data(
        make_metallic_roughness_texcoord1_transform_render_scene(false),
        config);
    if (!untransformed.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: "
                << untransformed.error);
        return;
    }
    auto transformed = Renderer3D::render_scene_data(
        make_metallic_roughness_texcoord1_transform_render_scene(true),
        config);

    INFO(untransformed.error);
    INFO(transformed.error);
    REQUIRE(untransformed.success);
    REQUIRE(transformed.success);
    REQUIRE(untransformed.metallic_roughness_texture_applied);
    REQUIRE(transformed.metallic_roughness_texture_applied);
    REQUIRE_FALSE(transformed.metallic_roughness_texture_deferred);
    REQUIRE(transformed.non_base_color_texture_transform_applied);
    REQUIRE(transformed.non_base_color_texcoord1_used);
    REQUIRE_FALSE(transformed.non_base_color_texture_transform_deferred);
    REQUIRE_FALSE(transformed.non_base_color_texcoord1_deferred);
    REQUIRE(transformed.texture_decoded);
    REQUIRE(transformed.fallback_texture_used);
    const HeadlessSurface::Rgba untransformed_rgba{
        untransformed.rgba,
        untransformed.width,
        untransformed.height,
    };
    const HeadlessSurface::Rgba transformed_rgba{
        transformed.rgba,
        transformed.width,
        transformed.height,
    };
    const uint64_t untransformed_fingerprint =
        HeadlessSurface::rgba_fingerprint(untransformed_rgba);
    const uint64_t transformed_fingerprint =
        HeadlessSurface::rgba_fingerprint(transformed_rgba);
    REQUIRE(untransformed_fingerprint != transformed_fingerprint);
    const double untransformed_luma =
        average_image_luma(untransformed.rgba, config.width, config.height);
    const double transformed_luma =
        average_image_luma(transformed.rgba, config.width, config.height);
    REQUIRE(untransformed_luma > transformed_luma + 4.0);
}

TEST_CASE("Renderer3D samples occlusion material texture",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto plain = Renderer3D::render_scene_data(
        make_metallic_roughness_factor_render_scene(0.0f, 0.1f),
        config);
    if (!plain.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " <<
                plain.error);
        return;
    }
    auto occluded = Renderer3D::render_scene_data(
        make_occlusion_texture_render_scene(),
        config);

    INFO(plain.error);
    INFO(occluded.error);
    REQUIRE(plain.success);
    REQUIRE(occluded.success);
    REQUIRE_FALSE(plain.occlusion_texture_applied);
    REQUIRE(occluded.occlusion_texture_applied);
    REQUIRE_FALSE(occluded.occlusion_strength_applied);
    REQUIRE_FALSE(occluded.occlusion_texture_deferred);
    REQUIRE_FALSE(occluded.occlusion_strength_deferred);
    REQUIRE(occluded.texture_decoded);
    REQUIRE(occluded.fallback_texture_used);
    const double plain_luma = average_foreground_luma(plain.rgba,
                                                      config.width,
                                                      config.height);
    const double occluded_luma = average_foreground_luma(occluded.rgba,
                                                         config.width,
                                                         config.height);
    REQUIRE(occluded_luma < plain_luma * 0.35);
}

TEST_CASE("Renderer3D applies occlusion texture strength",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto weak = Renderer3D::render_scene_data(
        make_occlusion_texture_render_scene(0.35f),
        config);
    if (!weak.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: " << weak.error);
        return;
    }
    auto full = Renderer3D::render_scene_data(
        make_occlusion_texture_render_scene(1.0f),
        config);

    INFO(weak.error);
    INFO(full.error);
    REQUIRE(weak.success);
    REQUIRE(full.success);
    REQUIRE(weak.occlusion_texture_applied);
    REQUIRE(full.occlusion_texture_applied);
    REQUIRE(weak.occlusion_strength_applied);
    REQUIRE_FALSE(full.occlusion_strength_applied);
    REQUIRE_FALSE(weak.occlusion_texture_deferred);
    REQUIRE_FALSE(weak.occlusion_strength_deferred);
    const double weak_luma = average_foreground_luma(weak.rgba,
                                                     config.width,
                                                     config.height);
    const double full_luma = average_foreground_luma(full.rgba,
                                                     config.width,
                                                     config.height);
    REQUIRE(full_luma < weak_luma * 0.60);
}

TEST_CASE("Renderer3D applies occlusion texture transform on TEXCOORD_1",
          "[render][scene3d][gpu]") {
    SceneDataRenderConfig config;
    config.width = 128;
    config.height = 128;

    auto untransformed = Renderer3D::render_scene_data(
        make_occlusion_texcoord1_transform_render_scene(false),
        config);
    if (!untransformed.gpu_available) {
        SUCCEED("Dawn/WebGPU unavailable in this environment: "
                << untransformed.error);
        return;
    }
    auto transformed = Renderer3D::render_scene_data(
        make_occlusion_texcoord1_transform_render_scene(true),
        config);

    INFO(untransformed.error);
    INFO(transformed.error);
    REQUIRE(untransformed.success);
    REQUIRE(transformed.success);
    REQUIRE(untransformed.occlusion_texture_applied);
    REQUIRE(transformed.occlusion_texture_applied);
    REQUIRE_FALSE(transformed.occlusion_texture_deferred);
    REQUIRE_FALSE(transformed.occlusion_strength_deferred);
    REQUIRE(transformed.non_base_color_texture_transform_applied);
    REQUIRE(transformed.non_base_color_texcoord1_used);
    REQUIRE_FALSE(transformed.non_base_color_texture_transform_deferred);
    REQUIRE_FALSE(transformed.non_base_color_texcoord1_deferred);
    REQUIRE(transformed.texture_decoded);
    REQUIRE(transformed.fallback_texture_used);
    const HeadlessSurface::Rgba untransformed_rgba{
        untransformed.rgba,
        untransformed.width,
        untransformed.height,
    };
    const HeadlessSurface::Rgba transformed_rgba{
        transformed.rgba,
        transformed.width,
        transformed.height,
    };
    const uint64_t untransformed_fingerprint =
        HeadlessSurface::rgba_fingerprint(untransformed_rgba);
    const uint64_t transformed_fingerprint =
        HeadlessSurface::rgba_fingerprint(transformed_rgba);
    REQUIRE(untransformed_fingerprint != transformed_fingerprint);
    const double untransformed_luma =
        average_foreground_luma(untransformed.rgba, config.width, config.height);
    const double transformed_luma =
        average_foreground_luma(transformed.rgba, config.width, config.height);
    REQUIRE(transformed_luma > untransformed_luma * 2.0);
}
