#include <catch2/catch_test_macros.hpp>

#include "tools/cli/create_targets.hpp"

#include <vector>

using pulp::cli::create_default_build_targets;

TEST_CASE("CLI create targets use format suffixes for plugin projects", "[cli][create]") {
    const auto targets =
        create_default_build_targets("PulpGain", "effect", "VST3 CLAP Standalone");

    CHECK(targets == std::vector<std::string>{
        "PulpGain-test",
        "PulpGain_VST3",
        "PulpGain_CLAP",
        "PulpGain_Standalone",
    });
}

TEST_CASE("CLI create targets use the app target name for standalone apps", "[cli][create]") {
    const auto targets = create_default_build_targets("PulpApp", "app", "Standalone");

    CHECK(targets == std::vector<std::string>{
        "PulpApp-test",
        "PulpApp",
    });
}

TEST_CASE("CLI create targets use the app target name for bare projects", "[cli][create]") {
    const auto targets = create_default_build_targets("PulpBare", "bare", "Standalone");

    CHECK(targets == std::vector<std::string>{
        "PulpBare-test",
        "PulpBare",
    });
}
