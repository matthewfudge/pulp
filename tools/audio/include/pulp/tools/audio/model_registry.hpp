#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pulp::tools::audio {

struct RegisteredModel {
    std::string model_id;
    std::string display_name;
    std::string backend;
    std::string checkpoint_ref;
    std::vector<std::string> task_tags;
    std::uint64_t size_bytes = 0;
};

const std::vector<RegisteredModel>& registered_models();
const RegisteredModel* find_registered_model(std::string_view model_id);

} // namespace pulp::tools::audio
