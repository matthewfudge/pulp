// pulp — CLI tool for the Pulp audio plugin framework
// Wraps common build/test/status operations

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ── Helpers ──────────────────────────────────────────────────────────────────

static int run(const std::string& cmd) {
    return std::system(cmd.c_str());
}

static std::string exec_output(const std::string& cmd) {
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    pclose(pipe);
    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

static fs::path find_project_root() {
    auto dir = fs::current_path();
    while (!dir.empty()) {
        if (fs::exists(dir / "CMakeLists.txt") && fs::exists(dir / "core")) {
            return dir;
        }
        auto parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return {};
}

// ── Commands ─────────────────────────────────────────────────────────────────

static int cmd_build(const std::vector<std::string>& args) {
    auto root = find_project_root();
    if (root.empty()) {
        std::cerr << "Error: not in a Pulp project directory\n";
        return 1;
    }

    auto build_dir = root / "build";
    bool needs_configure = !fs::exists(build_dir / "CMakeCache.txt");

    // Check if CMakeLists.txt is newer than CMakeCache
    if (!needs_configure && fs::exists(build_dir / "CMakeCache.txt")) {
        auto cmake_time = fs::last_write_time(root / "CMakeLists.txt");
        auto cache_time = fs::last_write_time(build_dir / "CMakeCache.txt");
        if (cmake_time > cache_time) needs_configure = true;
    }

    if (needs_configure) {
        std::cout << "Configuring...\n";
        int rc = run("cmake -B " + build_dir.string() + " -S " + root.string());
        if (rc != 0) return rc;
    }

    std::cout << "Building...\n";

    std::string build_cmd = "cmake --build " + build_dir.string();

    // Pass through extra args (e.g., --target, -j)
    for (auto& arg : args) {
        build_cmd += " " + arg;
    }

    return run(build_cmd);
}

static int cmd_test(const std::vector<std::string>& args) {
    auto root = find_project_root();
    if (root.empty()) {
        std::cerr << "Error: not in a Pulp project directory\n";
        return 1;
    }

    auto build_dir = root / "build";
    if (!fs::exists(build_dir / "CMakeCache.txt")) {
        std::cout << "Build directory not found, building first...\n";
        int rc = cmd_build({});
        if (rc != 0) return rc;
    }

    std::string test_cmd = "ctest --test-dir " + build_dir.string() + " --output-on-failure";

    for (auto& arg : args) {
        test_cmd += " " + arg;
    }

    return run(test_cmd);
}

static int cmd_status([[maybe_unused]] const std::vector<std::string>& args) {
    auto root = find_project_root();
    if (root.empty()) {
        std::cerr << "Error: not in a Pulp project directory\n";
        return 1;
    }

    // Read project info from CMakeLists.txt
    std::cout << "Pulp Project Status\n";
    std::cout << "====================\n";
    std::cout << "Root: " << root.string() << "\n";

    // Git info
    auto branch = exec_output("git -C " + root.string() + " branch --show-current");
    auto commit = exec_output("git -C " + root.string() + " log --oneline -1");
    if (!branch.empty()) std::cout << "Branch: " << branch << "\n";
    if (!commit.empty()) std::cout << "Commit: " << commit << "\n";

    // Build state
    auto build_dir = root / "build";
    if (fs::exists(build_dir / "CMakeCache.txt")) {
        std::cout << "Build: configured\n";
    } else {
        std::cout << "Build: not configured (run `pulp build`)\n";
    }

    // Count source files
    int cpp_count = 0, hpp_count = 0, test_count = 0;
    for (auto& entry : fs::recursive_directory_iterator(root / "core")) {
        auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".mm") ++cpp_count;
        if (ext == ".hpp" || ext == ".h") ++hpp_count;
    }
    for (auto& entry : fs::directory_iterator(root / "test")) {
        if (entry.path().extension() == ".cpp") ++test_count;
    }
    std::cout << "Source files: " << cpp_count << " impl, " << hpp_count << " headers\n";
    std::cout << "Test files: " << test_count << "\n";

    // Count examples
    int example_count = 0;
    if (fs::exists(root / "examples")) {
        for (auto& entry : fs::directory_iterator(root / "examples")) {
            if (entry.is_directory()) ++example_count;
        }
    }
    std::cout << "Examples: " << example_count << "\n";

    // Check for plugin formats
    std::cout << "\nPlugin Formats:\n";
    if (fs::exists(root / "external" / "vst3sdk"))
        std::cout << "  VST3: available\n";
    else
        std::cout << "  VST3: SDK not found\n";

    if (fs::exists(root / "external" / "AudioUnitSDK"))
        std::cout << "  AU:   available\n";
    else
        std::cout << "  AU:   SDK not found\n";

    std::cout << "  CLAP: available (fetched via CMake)\n";

    return 0;
}

static int cmd_clean([[maybe_unused]] const std::vector<std::string>& args) {
    auto root = find_project_root();
    if (root.empty()) {
        std::cerr << "Error: not in a Pulp project directory\n";
        return 1;
    }

    auto build_dir = root / "build";
    if (fs::exists(build_dir)) {
        std::cout << "Removing build directory...\n";
        fs::remove_all(build_dir);
        std::cout << "Clean.\n";
    } else {
        std::cout << "Nothing to clean.\n";
    }
    return 0;
}

static int cmd_validate(const std::vector<std::string>& args) {
    auto root = find_project_root();
    if (root.empty()) {
        std::cerr << "Error: not in a Pulp project directory\n";
        return 1;
    }

    auto build_dir = root / "build";
    if (!fs::exists(build_dir)) {
        std::cerr << "Build directory not found. Run `pulp build` first.\n";
        return 1;
    }

    int total = 0, passed = 0, failed = 0, skipped = 0;

    // CLAP validation
    auto clap_dir = build_dir / "CLAP";
    if (fs::exists(clap_dir)) {
        bool has_clap_validator = !exec_output("which clap-validator 2>/dev/null").empty();

        for (auto& entry : fs::directory_iterator(clap_dir)) {
            if (entry.path().extension() == ".clap") {
                auto name = entry.path().stem().string();
                ++total;

                if (has_clap_validator) {
                    std::cout << "CLAP: validating " << name << "... ";
                    auto clap_path = entry.path().string();
                    int rc = run("clap-validator validate \"" + clap_path + "\" 2>/dev/null");
                    if (rc == 0) {
                        std::cout << "PASSED\n";
                        ++passed;
                    } else {
                        std::cout << "FAILED\n";
                        ++failed;
                    }
                } else {
                    // Fallback: dlopen test
                    std::cout << "CLAP: " << name << " (dlopen check only, clap-validator not installed)... ";
                    auto test_cmd = "ctest --test-dir " + build_dir.string() + " -R clap-dlopen-" + name + " --output-on-failure 2>/dev/null";
                    int rc = run(test_cmd);
                    if (rc == 0) {
                        std::cout << "PASSED\n";
                        ++passed;
                    } else {
                        std::cout << "FAILED\n";
                        ++failed;
                    }
                }
            }
        }
    }

    // AU validation (macOS only)
#ifdef __APPLE__
    auto au_dir = build_dir / "AU";
    if (fs::exists(au_dir)) {
        for (auto& entry : fs::directory_iterator(au_dir)) {
            if (entry.path().extension() == ".component") {
                auto name = entry.path().stem().string();
                ++total;
                std::cout << "AU: " << name << " (auval check)... ";

                // Check if auval exists
                if (!exec_output("which auval 2>/dev/null").empty()) {
                    // Run auval test from ctest
                    auto test_cmd = "ctest --test-dir " + build_dir.string() + " -R auval-" + name + " --output-on-failure 2>/dev/null";
                    int rc = run(test_cmd);
                    if (rc == 0) {
                        std::cout << "PASSED\n";
                        ++passed;
                    } else {
                        std::cout << "FAILED\n";
                        ++failed;
                    }
                } else {
                    std::cout << "SKIPPED (auval not found)\n";
                    ++skipped;
                }
            }
        }
    }
#endif

    std::cout << "\nValidation Summary: " << total << " total, "
              << passed << " passed, " << failed << " failed, "
              << skipped << " skipped\n";

    return failed > 0 ? 1 : 0;
}

static int cmd_ship(const std::vector<std::string>& args) {
    auto root = find_project_root();
    if (root.empty()) {
        std::cerr << "Error: not in a Pulp project directory\n";
        return 1;
    }

    auto build_dir = root / "build";
    if (!fs::exists(build_dir / "CMakeCache.txt")) {
        std::cerr << "Build directory not found. Run `pulp build` first.\n";
        return 1;
    }

    // Parse subcommand
    std::string sub = args.empty() ? "help" : args[0];

    if (sub == "sign") {
        // Sign all plugin bundles
        std::string identity;
        std::string entitlements = (root / "ship" / "templates" / "entitlements.plist").string();
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--identity" && i + 1 < args.size())
                identity = args[++i];
            else if (args[i] == "--entitlements" && i + 1 < args.size())
                entitlements = args[++i];
        }
        if (identity.empty()) {
            std::cerr << "Usage: pulp ship sign --identity \"Developer ID Application: ...\"\n";
            return 1;
        }

        int signed_count = 0;
        for (auto dir_name : {"VST3", "CLAP", "AU"}) {
            auto dir = build_dir / dir_name;
            if (!fs::exists(dir)) continue;
            for (auto& entry : fs::directory_iterator(dir)) {
                auto ext = entry.path().extension().string();
                if (ext == ".vst3" || ext == ".clap" || ext == ".component") {
                    std::cout << "Signing " << entry.path().filename().string() << "...\n";
                    std::string cmd = "codesign --force --sign \"" + identity
                        + "\" --timestamp --options runtime"
                        + " --entitlements \"" + entitlements + "\""
                        + " \"" + entry.path().string() + "\"";
                    if (run(cmd) == 0) ++signed_count;
                    else std::cerr << "  FAILED\n";
                }
            }
        }
        std::cout << "Signed " << signed_count << " bundles.\n";
        return 0;
    }

    if (sub == "package") {
        auto artifacts = root / "artifacts";
        fs::create_directories(artifacts);

        std::string version = "0.1.0";
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--version" && i + 1 < args.size())
                version = args[++i];
        }

        int pkg_count = 0;
        for (auto dir_name : {"VST3", "CLAP", "AU"}) {
            auto dir = build_dir / dir_name;
            if (!fs::exists(dir)) continue;
            std::string format_lower = dir_name;
            for (auto& c : format_lower) c = static_cast<char>(std::tolower(c));

            for (auto& entry : fs::directory_iterator(dir)) {
                auto ext = entry.path().extension().string();
                if (ext == ".vst3" || ext == ".clap" || ext == ".component") {
                    auto name = entry.path().stem().string();
                    auto pkg_name = name + "-" + dir_name + "-" + version + ".pkg";
                    auto pkg_path = artifacts / pkg_name;

                    std::string install_loc = "/Library/Audio/Plug-Ins/";
                    if (ext == ".vst3") install_loc += "VST3/";
                    else if (ext == ".clap") install_loc += "CLAP/";
                    else install_loc = std::string(getenv("HOME") ? getenv("HOME") : "~")
                                     + "/Library/Audio/Plug-Ins/Components/";

                    std::cout << "Packaging " << name << " (" << dir_name << ")...\n";
                    std::string cmd = "pkgbuild --component \"" + entry.path().string() + "\""
                        + " --identifier \"com.pulp." + name + "." + format_lower + "\""
                        + " --version \"" + version + "\""
                        + " --install-location \"" + install_loc + "\""
                        + " \"" + pkg_path.string() + "\" 2>/dev/null";
                    if (run(cmd) == 0) ++pkg_count;
                    else std::cerr << "  FAILED\n";
                }
            }
        }
        std::cout << "Created " << pkg_count << " packages in " << artifacts.string() << "\n";
        return 0;
    }

    if (sub == "check") {
        // Check signing status of all built plugins
        for (auto dir_name : {"VST3", "CLAP", "AU"}) {
            auto dir = build_dir / dir_name;
            if (!fs::exists(dir)) continue;
            for (auto& entry : fs::directory_iterator(dir)) {
                auto ext = entry.path().extension().string();
                if (ext == ".vst3" || ext == ".clap" || ext == ".component") {
                    std::cout << entry.path().filename().string() << ": ";
                    int rc = run("codesign --verify --deep --strict \""
                                 + entry.path().string() + "\" 2>/dev/null");
                    std::cout << (rc == 0 ? "signed" : "unsigned") << "\n";
                }
            }
        }
        return 0;
    }

    // Help
    std::cout << "pulp ship — signing and packaging commands\n\n";
    std::cout << "Subcommands:\n";
    std::cout << "  sign     Sign all plugin bundles\n";
    std::cout << "           --identity \"Developer ID Application: ...\"\n";
    std::cout << "           --entitlements path/to/entitlements.plist\n";
    std::cout << "  package  Create .pkg installers for all built plugins\n";
    std::cout << "           --version 1.0.0\n";
    std::cout << "  check    Check signing status of built plugins\n";
    return 0;
}

static void print_usage() {
    std::cout << "pulp — Pulp audio plugin framework CLI\n\n";
    std::cout << "Usage: pulp <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  build    Build the project (configure + compile)\n";
    std::cout << "  test     Run the test suite\n";
    std::cout << "  status   Show project status and info\n";
    std::cout << "  validate Run plugin format validators (clap-validator, auval)\n";
    std::cout << "  ship     Sign, package, and check plugins\n";
    std::cout << "  clean    Remove build directory\n";
    std::cout << "  help     Show this help\n";
    std::cout << "\nExamples:\n";
    std::cout << "  pulp build              # Build all targets\n";
    std::cout << "  pulp build --target X   # Build specific target\n";
    std::cout << "  pulp test               # Run all tests\n";
    std::cout << "  pulp test -R Knob       # Run tests matching 'Knob'\n";
    std::cout << "  pulp ship sign --identity \"Developer ID Application: Foo\"\n";
    std::cout << "  pulp ship package --version 1.0.0\n";
    std::cout << "  pulp status             # Show project info\n";
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    std::string command = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i)
        args.push_back(argv[i]);

    if (command == "build")    return cmd_build(args);
    if (command == "test")     return cmd_test(args);
    if (command == "status")   return cmd_status(args);
    if (command == "validate") return cmd_validate(args);
    if (command == "ship")     return cmd_ship(args);
    if (command == "clean")    return cmd_clean(args);
    if (command == "help" || command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }

    std::cerr << "Unknown command: " << command << "\n";
    std::cerr << "Run `pulp help` for usage\n";
    return 1;
}
