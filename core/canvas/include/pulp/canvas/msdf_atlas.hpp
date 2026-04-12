#pragma once

// Multi-Channel Signed Distance Field (MSDF) glyph atlas.
//
// An MSDF encodes three independent distance signals into the R, G, B
// channels of each texel. At sample time the shader reconstructs the
// true distance via `median(r, g, b)`, which preserves sharp corners
// at arbitrary scales — something single-channel SDF cannot do without
// becoming a path-rendered fallback.
//
// Algorithm reference:
//   Chlumský, V. (2015) — "Shape decomposition for multi-channel
//   distance fields." MSc thesis, Charles University.
//   https://github.com/Chlumsky/msdfgen (BSD-2-Clause)
//
// `MsdfAtlas` mirrors the `SdfAtlas` API so call-sites can switch
// between single and multi-channel rendering with only a shader
// swap. The underlying pixel buffer is RGB8 (3 bytes per texel).

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pulp::canvas {

struct MsdfGlyph {
    char32_t codepoint = 0;
    int      atlas_x = 0;
    int      atlas_y = 0;
    int      width   = 0;
    int      height  = 0;
    float    bearing_x = 0.0f;
    float    bearing_y = 0.0f;
    float    advance   = 0.0f;
};

class MsdfAtlas {
public:
    MsdfAtlas();
    ~MsdfAtlas();

    MsdfAtlas(const MsdfAtlas&) = delete;
    MsdfAtlas& operator=(const MsdfAtlas&) = delete;
    MsdfAtlas(MsdfAtlas&&) noexcept;
    MsdfAtlas& operator=(MsdfAtlas&&) noexcept;

    // Build the atlas for `chars` at `base_size` with `padding` texels of
    // SDF spread around each glyph. Returns false if the font cannot be
    // resolved or the resulting atlas would exceed `max_atlas_size`.
    bool build(const std::string& font_family,
               const std::vector<char32_t>& chars,
               int base_size = 48,
               int padding   = 8,
               int max_atlas_size = 2048);

    const MsdfGlyph* glyph(char32_t codepoint) const;

    // RGB8 atlas image: width() * height() * 3 bytes, row-major.
    const std::uint8_t* pixels() const;
    int width()  const;
    int height() const;
    std::size_t glyph_count() const;
    int base_size() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace pulp::canvas
