#include <pulp/canvas/path_to_sdf.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace pulp::canvas {

namespace {

// Felzenszwalb-Huttenlocher 1D squared-Euclidean distance transform.
// Runs in linear time per row/column.
void edt_1d(const float* f, float* d, int n, int* v, float* z) {
    const float INF = std::numeric_limits<float>::infinity();
    int k = 0;
    v[0] = 0;
    z[0] = -INF;
    z[1] =  INF;
    for (int q = 1; q < n; ++q) {
        float s = ((f[q] + static_cast<float>(q * q)) -
                   (f[v[k]] + static_cast<float>(v[k] * v[k])))
                  / static_cast<float>(2 * q - 2 * v[k]);
        while (s <= z[k]) {
            --k;
            s = ((f[q] + static_cast<float>(q * q)) -
                 (f[v[k]] + static_cast<float>(v[k] * v[k])))
                / static_cast<float>(2 * q - 2 * v[k]);
        }
        ++k;
        v[k]     = q;
        z[k]     = s;
        z[k + 1] = INF;
    }
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (z[k + 1] < static_cast<float>(q)) ++k;
        const float dq = static_cast<float>(q - v[k]);
        d[q] = dq * dq + f[v[k]];
    }
}

std::vector<float> distance_transform(const std::uint8_t* mask,
                                      int w, int h, bool inside) {
    const float INF = std::numeric_limits<float>::infinity();
    std::vector<float> f(static_cast<std::size_t>(w * h));
    for (int i = 0; i < w * h; ++i) {
        const bool set = inside ? (mask[i] > 127) : (mask[i] <= 127);
        f[i] = set ? 0.0f : INF;
    }

    const int max_dim = std::max(w, h);
    std::vector<float> d_col(static_cast<std::size_t>(h));
    std::vector<float> f_col(static_cast<std::size_t>(h));
    std::vector<int>   v(static_cast<std::size_t>(max_dim));
    std::vector<float> z(static_cast<std::size_t>(max_dim + 1));

    // Columns
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) f_col[y] = f[y * w + x];
        edt_1d(f_col.data(), d_col.data(), h, v.data(), z.data());
        for (int y = 0; y < h; ++y) f[y * w + x] = d_col[y];
    }
    // Rows
    std::vector<float> d_row(static_cast<std::size_t>(w));
    std::vector<float> f_row(static_cast<std::size_t>(w));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) f_row[x] = f[y * w + x];
        edt_1d(f_row.data(), d_row.data(), w, v.data(), z.data());
        for (int x = 0; x < w; ++x) f[y * w + x] = d_row[x];
    }
    return f;
}

}  // namespace

std::vector<std::uint8_t> path_to_sdf(const std::uint8_t* mask,
                                      int width, int height,
                                      int spread) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(width * height), 0);
    if (width <= 0 || height <= 0 || spread <= 0) return out;

    // Guard the degenerate empty / fully-filled cases — EDT has no
    // finite seeds to work from and edt_1d would produce NaNs.
    bool any_inside = false;
    bool any_outside = false;
    for (int i = 0; i < width * height; ++i) {
        if (mask[i] > 127) any_inside = true;
        else               any_outside = true;
        if (any_inside && any_outside) break;
    }
    if (!any_inside) {  // all-outside → saturate to 0
        std::fill(out.begin(), out.end(), static_cast<std::uint8_t>(0));
        return out;
    }
    if (!any_outside) {  // all-inside → saturate to 255
        std::fill(out.begin(), out.end(), static_cast<std::uint8_t>(255));
        return out;
    }

    const auto d_out = distance_transform(mask, width, height, /*inside*/ true);
    const auto d_in  = distance_transform(mask, width, height, /*inside*/ false);

    for (int i = 0; i < width * height; ++i) {
        const float dist_out = std::sqrt(d_out[i]);
        const float dist_in  = std::sqrt(d_in[i]);
        const float signed_d = dist_out - dist_in;  // pos outside, neg inside
        const float norm = 0.5f - 0.5f * signed_d / static_cast<float>(spread);
        out[i] = static_cast<std::uint8_t>(
            std::clamp(norm * 255.0f, 0.0f, 255.0f));
    }
    return out;
}

}  // namespace pulp::canvas
