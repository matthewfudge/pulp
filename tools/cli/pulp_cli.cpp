// pulp — CLI tool for the Pulp audio plugin framework
// Command dispatch via structured table. See cmd_*.cpp for implementations.

#include "cli_common.hpp"

#include <cstring>
#include <iomanip>
#include <iostream>

// ── Command Table ───────────────────────────────────────────────────────────

struct Command {
    const char* name;
    const char* summary;
    int (*handler)(const std::vector<std::string>&);
};

static const Command commands[] = {
    {"build",    "Configure and build the project",       cmd_build},
    {"run",      "Launch a standalone Pulp application",  cmd_run},
    {"test",     "Run the test suite",                    cmd_test},
    {"status",   "Show project status and info",          cmd_status},
    {"create",   "Scaffold a new plugin project",         cmd_create},
    {"validate", "Run plugin format validators",          cmd_validate},
    {"doctor",   "Diagnose environment issues",           cmd_doctor},
    {"ship",     "Sign, package, and distribute",         cmd_ship},
    {"design",   "Launch the AI design tool",             cmd_design},
    {"docs",     "Browse local documentation",            cmd_docs},
    {"clean",    "Remove build directory",                cmd_clean},
    {"cache",    "Manage SDK and asset cache",            cmd_cache},
    {"upgrade",  "Update the CLI to the latest version",  cmd_upgrade},
};

static constexpr int command_count = sizeof(commands) / sizeof(commands[0]);

// Script commands: delegate to a Python script in the project tree
struct ScriptCommand {
    const char* name;
    const char* script_path;  // relative to project root
    const char* summary;
};

static const ScriptCommand script_commands[] = {
    {"add",      "tools/add-component.py",          "Add a component to the project"},
    {"audit",    "tools/audit.py",                   "License and clean-room audit"},
    {"ci-local", "tools/local-ci/local_ci.py",       "Local-first CI across configured hosts"},
};

static constexpr int script_command_count = sizeof(script_commands) / sizeof(script_commands[0]);

// Binary commands: delegate to a built binary
struct BinaryCommand {
    const char* name;
    const char* binary_path;  // relative to project build dir
    const char* summary;
    const char* extra_arg;    // prepended arg (e.g., "--demo"), or nullptr
};

static const BinaryCommand binary_commands[] = {
    {"design-debug",  "tools/design/pulp-design-debug",            "Headless design debug runner", nullptr},
    {"inspect",       "tools/screenshot/pulp-screenshot",          "Launch the component inspector", "--demo"},
    {"import-design", "tools/import-design/pulp-import-design",    "Import designs from Figma/Stitch/v0/Pencil", nullptr},
    {"export-tokens", "tools/import-design/pulp-import-design",    "Export theme as W3C Design Tokens", "--export-tokens"},
};

static constexpr int binary_command_count = sizeof(binary_commands) / sizeof(binary_commands[0]);

// ── Helpers ─────────────────────────────────────────────────────────────────

static int delegate_to_script(const ScriptCommand& sc, const std::vector<std::string>& args) {
    auto root = require_project_root();
    if (!root) return 1;
    auto script = *root / sc.script_path;
    if (!fs::exists(script)) {
        std::cerr << "Error: script not found at " << script.string() << "\n";
        return 1;
    }
    std::string cmd = "python3 " + shell_quote(script);
    for (auto& arg : args) cmd += " " + shell_quote(arg);
    return run(cmd);
}

static int delegate_to_binary(const BinaryCommand& bc, const std::vector<std::string>& args) {
    auto root = require_project_root();
    if (!root) return 1;
    auto binary = *root / "build" / bc.binary_path;
    if (!fs::exists(binary)) {
        std::cerr << "Error: " << fs::path(bc.binary_path).filename().string()
                  << " not built. Run `pulp build` first.\n";
        return 1;
    }
    std::string cmd = shell_quote(binary);
    if (bc.extra_arg) cmd += std::string(" ") + bc.extra_arg;
    for (auto& arg : args) cmd += " " + shell_quote(arg);
    return run(cmd);
}

// ── Usage (auto-generated from command tables) ──────────────────────────────

static void print_usage() {
    std::cout << "pulp — Pulp audio plugin framework CLI\n\n";
    std::cout << "Usage: pulp <command> [options]\n\n";
    std::cout << "Commands:\n";
    for (int i = 0; i < command_count; ++i) {
        std::cout << "  " << std::left << std::setw(14) << commands[i].name
                  << " " << commands[i].summary << "\n";
    }
    std::cout << "\n";
    for (int i = 0; i < script_command_count; ++i) {
        std::cout << "  " << std::left << std::setw(14) << script_commands[i].name
                  << " " << script_commands[i].summary << "\n";
    }
    for (int i = 0; i < binary_command_count; ++i) {
        std::cout << "  " << std::left << std::setw(14) << binary_commands[i].name
                  << " " << binary_commands[i].summary << "\n";
    }
    std::cout << "  " << std::left << std::setw(14) << "help" << " Show this help\n";
    std::cout << "\nExamples:\n";
    std::cout << "  pulp create MyPlugin              # Create a new effect plugin\n";
    std::cout << "  pulp create MySynth --type instrument  # Create an instrument\n";
    std::cout << "  pulp doctor             # Check environment for issues\n";
    std::cout << "  pulp build              # Build all targets\n";
    std::cout << "  pulp test               # Run all tests\n";
    std::cout << "  pulp validate           # Validate built plugins\n";
    std::cout << "  pulp docs index         # List available docs\n";
    std::cout << "  pulp status             # Show project info\n";
}

// ── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--no-color") == 0) {
            g_no_color = true;
        }
    }
    init_color();

    if (argc < 2) {
        print_usage();
        return 0;
    }

    std::string command = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--no-color") == 0) continue;
        args.push_back(argv[i]);
    }

    // Lookup in command table
    for (int i = 0; i < command_count; ++i) {
        if (command == commands[i].name) return commands[i].handler(args);
    }

    // Lookup in script commands
    for (int i = 0; i < script_command_count; ++i) {
        if (command == script_commands[i].name) return delegate_to_script(script_commands[i], args);
    }

    // Lookup in binary commands
    for (int i = 0; i < binary_command_count; ++i) {
        if (command == binary_commands[i].name) return delegate_to_binary(binary_commands[i], args);
    }

    // Help
    if (command == "help" || command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }

    std::cerr << "Unknown command: " << command << "\n";
    std::cerr << "Run `pulp help` for usage\n";
    return 1;
}
