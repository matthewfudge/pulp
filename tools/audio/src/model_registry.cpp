#include <pulp/tools/audio/model_registry.hpp>

namespace pulp::tools::audio {

// The audio model catalog. The registry type + URL resolution + lookup are generic
// (pulp::runtime); only this data lives here.
const std::vector<RegisteredModel>& registered_models() {
    static const std::vector<RegisteredModel> models = {{
        .model_id = "clap_music_audioset_v1",
        .display_name = "CLAP Music AudioSet",
        .backend = "clap",
        .checkpoint_ref = "hf://lukewys/laion_clap/music.pt",
        .task_tags = {"music", "excerpt_find"},
        .size_bytes = 2523293286ULL,
        .download_url = "https://huggingface.co/lukewys/laion_clap/resolve/main/music.pt",
        .sha256 = "",  // Populated after first verified download
        .auto_downloadable = true,
    }};
    return models;
}

}  // namespace pulp::tools::audio
