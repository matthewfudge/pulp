#pragma once

#include <string>
#include <vector>

namespace pulp::cli {

std::vector<std::string> create_default_build_targets(const std::string& class_name,
                                                      const std::string& type,
                                                      const std::string& formats);

}  // namespace pulp::cli
