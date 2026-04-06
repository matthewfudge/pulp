#pragma once

#include <pulp/audio/audio_file.hpp>
#include <pulp/audio/excerpt_types.hpp>
#include <cstdint>
#include <vector>

namespace pulp::audio {

enum class WindowEnumerationStatus {
    ok,
    invalid_query,
    empty_audio,
    file_too_short,
};

struct WindowEnumerationResult {
    WindowEnumerationStatus status = WindowEnumerationStatus::invalid_query;
    std::vector<ExcerptWindow> windows;
    uint64_t source_frames = 0;
};

WindowEnumerationResult enumerate_excerpt_windows(const std::string& source_path,
                                                  const AudioFileData& audio,
                                                  const ExcerptQuery& query);

} // namespace pulp::audio
