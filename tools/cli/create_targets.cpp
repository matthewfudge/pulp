#include "create_targets.hpp"

#include <algorithm>

namespace pulp::cli {

std::vector<std::string> create_default_build_targets(const std::string& class_name,
                                                      const std::string& type,
                                                      const std::string& formats) {
    std::vector<std::string> targets;
    auto add_target = [&](const std::string& target) {
        if (!target.empty() &&
            std::find(targets.begin(), targets.end(), target) == targets.end()) {
            targets.push_back(target);
        }
    };

    add_target(class_name + "-test");

    auto add_format_target = [&](const std::string& format, const std::string& suffix) {
        if (formats.find(format) != std::string::npos) {
            add_target(class_name + "_" + suffix);
        }
    };

    add_format_target("VST3", "VST3");
    add_format_target("CLAP", "CLAP");
    add_format_target("AU", "AU");
    add_format_target("AUv3", "AUv3");
    add_format_target("LV2", "LV2");
    add_format_target("AAX", "AAX");

    if (formats.find("Standalone") != std::string::npos) {
        if (type == "app" || type == "bare") {
            add_target(class_name);
        } else {
            add_target(class_name + "_Standalone");
        }
    }

    return targets;
}

}  // namespace pulp::cli
