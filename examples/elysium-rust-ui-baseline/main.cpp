#include "elysium_imported_ui.hpp"

#include <pulp/view/view.hpp>
#include <pulp/view/window_host.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#if defined(__unix__)
#include <unistd.h>
#endif

namespace {

std::filesystem::path asset_dir() {
    if (const char* env = std::getenv("PULP_ELYSIUM_RUIF_ASSET_DIR")) {
        if (*env != '\0')
            return env;
    }
    return PULP_ELYSIUM_RUIF_ASSET_DIR;
}

bool set_working_directory(const std::filesystem::path& dir) {
#if defined(__unix__)
    return ::chdir(dir.c_str()) == 0;
#else
    std::error_code ec;
    std::filesystem::current_path(dir, ec);
    return !ec;
#endif
}

bool write_png(const std::filesystem::path& path, const std::vector<uint8_t>& png) {
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open())
        return false;
    out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    return out.good();
}

}  // namespace

int main(int argc, char** argv) {
    std::filesystem::path screenshot_path;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        constexpr std::string_view prefix = "--screenshot=";
        if (arg.rfind(prefix, 0) == 0) {
            screenshot_path = arg.substr(prefix.size());
        }
    }

    const auto assets = asset_dir();
    if (!std::filesystem::exists(assets / "assets")) {
        std::cerr << "ELYSIUM assets not found at " << (assets / "assets") << "\n";
        return 1;
    }
    if (!set_working_directory(assets)) {
        std::cerr << "failed to set ELYSIUM asset working directory: " << assets << "\n";
        return 1;
    }

    auto root = build_imported_ui();
    if (!root) {
        std::cerr << "failed to build ELYSIUM imported UI\n";
        return 1;
    }
    root->set_requires_gpu_host(true);
    root->set_bounds({0.0f, 0.0f, 1000.0f, 600.0f});
    root->layout_children();

    pulp::view::WindowOptions options;
    options.title = "Pulp ELYSIUM RUIF C++ Baseline";
    options.width = 1000.0f;
    options.height = 600.0f;
    options.min_width = 667.0f;
    options.min_height = 400.0f;
    options.resizable = true;
    options.use_gpu = true;
    options.initially_hidden = false;

    auto window = pulp::view::WindowHost::create(*root, options);
    if (!window) {
        std::cerr << "failed to create ELYSIUM GPU window host\n";
        return 1;
    }
    window->set_design_viewport(1000.0f, 600.0f);
    window->set_fixed_aspect_ratio(1000.0f / 600.0f);
    window->set_close_callback([] {});

    if (!screenshot_path.empty()) {
        int frame_count = 0;
        window->set_idle_callback([&] {
            if (++frame_count < 30)
                return;
            auto png = window->capture_png();
            if (png.empty() || !write_png(screenshot_path, png)) {
                std::cerr << "failed to capture ELYSIUM GPU screenshot: "
                          << screenshot_path << "\n";
            }
            window->request_close();
        });
    }

    window->run_event_loop();
    return 0;
}
