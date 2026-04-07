#include <catch2/catch_test_macros.hpp>
#include <pulp/canvas/image_convolution.hpp>
#include <vector>
#include <numeric>

using namespace pulp::canvas;

// Helper: create a test image (RGBA, solid color)
static std::vector<uint8_t> make_solid_image(int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    std::vector<uint8_t> pixels(static_cast<size_t>(w * h * 4));
    for (int i = 0; i < w * h; ++i) {
        pixels[static_cast<size_t>(i * 4)] = r;
        pixels[static_cast<size_t>(i * 4 + 1)] = g;
        pixels[static_cast<size_t>(i * 4 + 2)] = b;
        pixels[static_cast<size_t>(i * 4 + 3)] = 255;
    }
    return pixels;
}

TEST_CASE("ImageConvolutionKernel construction", "[canvas][convolution]") {
    ImageConvolutionKernel k(3);
    REQUIRE(k.size() == 3);

    // Even size forced to odd
    ImageConvolutionKernel k2(4);
    REQUIRE(k2.size() == 5);

    // Size 0 forced to 1
    ImageConvolutionKernel k3(0);
    REQUIRE(k3.size() == 1);
}

TEST_CASE("ImageConvolutionKernel set and get", "[canvas][convolution]") {
    ImageConvolutionKernel k(3);
    k.set(0, 0, 1.0f);
    k.set(1, 1, 5.0f);
    k.set(2, 2, -1.0f);

    REQUIRE(k.get(0, 0) == 1.0f);
    REQUIRE(k.get(1, 1) == 5.0f);
    REQUIRE(k.get(2, 2) == -1.0f);
    REQUIRE(k.get(0, 1) == 0.0f);  // default
}

TEST_CASE("Identity kernel preserves image", "[canvas][convolution]") {
    // Identity kernel: 0 0 0 / 0 1 0 / 0 0 0
    ImageConvolutionKernel identity(3);
    identity.set(1, 1, 1.0f);

    auto pixels = make_solid_image(4, 4, 128, 64, 32);
    auto original = pixels;

    identity.apply(pixels.data(), 4, 4);

    // All pixels should be unchanged
    for (size_t i = 0; i < pixels.size(); ++i) {
        REQUIRE(pixels[i] == original[i]);
    }
}

TEST_CASE("Gaussian blur smooths uniform image", "[canvas][convolution]") {
    // A uniform image should be unchanged by blur (all neighbors are the same)
    auto k = ImageConvolutionKernel::gaussian_blur_3();
    auto pixels = make_solid_image(8, 8, 100, 100, 100);
    auto original = pixels;

    k.apply(pixels.data(), 8, 8);

    // Center pixels should be nearly unchanged (edges may vary due to clamping)
    int center = 4 * 8 + 4;  // row 4, col 4
    int idx = center * 4;
    REQUIRE(std::abs(static_cast<int>(pixels[idx]) - 100) <= 1);
}

TEST_CASE("Blur reduces contrast of edge", "[canvas][convolution]") {
    // Create image with sharp edge: left half white, right half black
    int w = 16, h = 4;
    std::vector<uint8_t> pixels(static_cast<size_t>(w * h * 4));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t v = (x < w / 2) ? 255 : 0;
            int idx = (y * w + x) * 4;
            pixels[idx] = v; pixels[idx + 1] = v; pixels[idx + 2] = v; pixels[idx + 3] = 255;
        }
    }

    auto k = ImageConvolutionKernel::gaussian_blur_3();
    k.apply(pixels.data(), w, h);

    // Pixel at the edge (x=7, y=2) should be intermediate (not 255 or 0)
    int edge_idx = (2 * w + 7) * 4;
    REQUIRE(pixels[edge_idx] > 0);
    REQUIRE(pixels[edge_idx] < 255);
}

TEST_CASE("Apply on null/zero returns safely", "[canvas][convolution]") {
    auto k = ImageConvolutionKernel::gaussian_blur_3();
    k.apply(nullptr, 10, 10);  // should not crash
    uint8_t dummy[4] = {128, 128, 128, 255};
    k.apply(dummy, 0, 0);      // zero dimensions
    k.apply(dummy, -1, 10);    // negative dimensions
}

TEST_CASE("Alpha channel preserved", "[canvas][convolution]") {
    int w = 4, h = 4;
    std::vector<uint8_t> pixels(static_cast<size_t>(w * h * 4));
    for (int i = 0; i < w * h; ++i) {
        pixels[i * 4] = 100;
        pixels[i * 4 + 1] = 100;
        pixels[i * 4 + 2] = 100;
        pixels[i * 4 + 3] = 200;  // non-trivial alpha
    }

    auto k = ImageConvolutionKernel::gaussian_blur_3();
    k.apply(pixels.data(), w, h);

    // Alpha should be preserved (not convolved)
    for (int i = 0; i < w * h; ++i) {
        REQUIRE(pixels[i * 4 + 3] == 200);
    }
}

TEST_CASE("Standard kernels have correct size", "[canvas][convolution]") {
    REQUIRE(ImageConvolutionKernel::gaussian_blur_3().size() == 3);
    REQUIRE(ImageConvolutionKernel::gaussian_blur_5().size() == 5);
    REQUIRE(ImageConvolutionKernel::sharpen().size() == 3);
    REQUIRE(ImageConvolutionKernel::edge_detect().size() == 3);
    REQUIRE(ImageConvolutionKernel::emboss().size() == 3);
}

TEST_CASE("Sharpen kernel center value is positive", "[canvas][convolution]") {
    auto k = ImageConvolutionKernel::sharpen();
    REQUIRE(k.get(1, 1) > 0.0f);  // center should amplify
}
