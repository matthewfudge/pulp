#pragma once

#include <pulp/scene/scene_data.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace pulp::scene {

struct DracoAttributeIds {
    int position = -1;
    int normal = -1;
    int texcoord0 = -1;
    int texcoord1 = -1;
    int tangent = -1;
    int color0 = -1;
};

struct DracoDecodedMesh {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> texcoord0;
    std::vector<float> texcoord1;
    std::vector<float> tangents;
    std::vector<float> color0;
    std::vector<uint32_t> indices;
    bool success = false;
    bool decoder_available = true;
};

using DracoDecodeCallback = std::function<DracoDecodedMesh(
    const uint8_t* data,
    size_t size,
    const DracoAttributeIds& attribute_ids)>;

struct LoadOptions {
    bool allow_draco = true;
    DracoDecodeCallback draco_decode;
};

struct LoadResult {
    SceneData scene;
    bool success = false;
    std::string error;
};

// Native glTF/GLB loader boundary. The implementation will keep fastgltf/cgltf
// private and return only engine-owned SceneData.
LoadResult load_gltf_scene(const std::filesystem::path& path,
                           const LoadOptions& options = {});

} // namespace pulp::scene
