#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace pulp::audio {

struct ExcerptQuery {
    std::string text;
    uint64_t window_frames = 0;
    uint64_t hop_frames = 0;
    std::size_t top_k = 5;
    float min_score = 0.0f;
};

struct ExcerptWindow {
    std::string source_path;
    uint32_t sample_rate = 0;
    uint64_t start_frame = 0;
    uint64_t frame_count = 0;

    [[nodiscard]] uint64_t end_frame() const { return start_frame + frame_count; }
    [[nodiscard]] double start_seconds() const {
        return sample_rate == 0 ? 0.0 : static_cast<double>(start_frame) / static_cast<double>(sample_rate);
    }
    [[nodiscard]] double duration_seconds() const {
        return sample_rate == 0 ? 0.0 : static_cast<double>(frame_count) / static_cast<double>(sample_rate);
    }
};

struct ExcerptCandidate {
    ExcerptWindow window;
    double score = 0.0;
    std::string backend;
    std::string model_id;
};

struct ExcerptSearchSummary {
    std::string query_text;
    std::size_t input_file_count = 0;
    uint64_t total_frames_scanned = 0;
    std::size_t enumerated_window_count = 0;
    std::size_t ranked_candidate_count = 0;
};

} // namespace pulp::audio
