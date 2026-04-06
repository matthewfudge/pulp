#include <pulp/tools/audio/model_registry.hpp>

namespace pulp::tools::audio {

const std::vector<RegisteredModel>& registered_models() {
    static const std::vector<RegisteredModel> models = {{
        .model_id = "clap_music_audioset_v1",
        .display_name = "CLAP Music AudioSet",
        .backend = "clap",
        .checkpoint_ref = "hf://lukewys/laion_clap/music.pt",
        .task_tags = {"music", "excerpt_find"},
        .size_bytes = 2523293286ULL,
    }};
    return models;
}

const RegisteredModel* find_registered_model(std::string_view model_id) {
    for (const auto& model : registered_models()) {
        if (model.model_id == model_id) return &model;
    }
    return nullptr;
}

} // namespace pulp::tools::audio
