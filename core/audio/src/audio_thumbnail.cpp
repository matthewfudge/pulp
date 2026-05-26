#include <pulp/audio/audio_thumbnail.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace pulp::audio {

// ─────────────────────────────────────────────────────────────────────────
// AudioThumbnail
// ─────────────────────────────────────────────────────────────────────────

static ThumbnailLevel decimate_level(const ThumbnailLevel& src) {
    ThumbnailLevel out;
    out.samples_per_peak = src.samples_per_peak * 2u;
    const std::size_t in_peaks = src.peaks_per_channel;
    const std::size_t out_peaks = (in_peaks + 1u) / 2u;
    out.peaks_per_channel = static_cast<uint32_t>(out_peaks);
    out.peaks.resize(src.peaks.size());

    for (std::size_t ch = 0; ch < src.peaks.size(); ++ch) {
        const auto& in = src.peaks[ch];
        auto& dst = out.peaks[ch];
        dst.resize(out_peaks);
        for (std::size_t i = 0; i < out_peaks; ++i) {
            const std::size_t a = i * 2;
            const std::size_t b = std::min(a + 1, in_peaks - 1);
            const AudioPeak& pa = in[a];
            const AudioPeak& pb = in[b];
            AudioPeak merged;
            merged.min_q7 = std::min(pa.min_q7, pb.min_q7);
            merged.max_q7 = std::max(pa.max_q7, pb.max_q7);
            dst[i] = merged;
        }
    }
    return out;
}

AudioThumbnail AudioThumbnail::build_from_buffer(
    const AudioFileData& data,
    uint32_t samples_per_peak) {
    AudioThumbnail t;
    if (data.empty() || samples_per_peak == 0) {
        return t;
    }

    t.num_channels_ = data.num_channels();
    t.num_source_frames_ = data.num_frames();
    t.sample_rate_ = data.sample_rate;

    // Base level.
    ThumbnailLevel base;
    base.samples_per_peak = samples_per_peak;
    const uint64_t frames = data.num_frames();
    const uint32_t peaks =
        static_cast<uint32_t>((frames + samples_per_peak - 1u) / samples_per_peak);
    base.peaks_per_channel = peaks;
    base.peaks.resize(data.num_channels());

    for (std::size_t ch = 0; ch < data.num_channels(); ++ch) {
        const auto& src = data.channels[ch];
        auto& dst = base.peaks[ch];
        dst.resize(peaks);
        for (uint32_t p = 0; p < peaks; ++p) {
            const std::size_t a = static_cast<std::size_t>(p) * samples_per_peak;
            const std::size_t b = std::min<std::size_t>(a + samples_per_peak, src.size());
            float lo =  std::numeric_limits<float>::infinity();
            float hi = -std::numeric_limits<float>::infinity();
            for (std::size_t i = a; i < b; ++i) {
                const float s = src[i];
                if (s < lo) lo = s;
                if (s > hi) hi = s;
            }
            if (!std::isfinite(lo) || !std::isfinite(hi)) {
                lo = 0.0f;
                hi = 0.0f;
            }
            dst[p] = AudioPeak::from_range(lo, hi);
        }
    }
    t.levels_.push_back(std::move(base));

    // Decimate until level holds <= 16 peaks per channel.
    while (t.levels_.back().peaks_per_channel > 16u) {
        t.levels_.push_back(decimate_level(t.levels_.back()));
    }

    return t;
}

std::optional<AudioThumbnail> AudioThumbnail::build_from_path(
    const std::string& path,
    uint32_t samples_per_peak) {
    auto data = read_audio_file(path);
    if (!data) return std::nullopt;
    if (data->empty()) return std::nullopt;
    return build_from_buffer(*data, samples_per_peak);
}

AudioThumbnailInfo AudioThumbnail::info() const noexcept {
    AudioThumbnailInfo i;
    i.num_channels = num_channels_;
    i.num_source_frames = num_source_frames_;
    i.sample_rate = sample_rate_;
    i.num_levels = levels_.size();
    std::size_t bytes = 0;
    for (const auto& lvl : levels_) {
        for (const auto& ch : lvl.peaks) {
            bytes += ch.size() * sizeof(AudioPeak);
        }
    }
    i.bytes_used = bytes;
    return i;
}

std::size_t AudioThumbnail::best_level_for(uint32_t target_peaks) const noexcept {
    if (levels_.empty()) return 0;
    if (target_peaks == 0) return levels_.size() - 1;
    // Walk from coarsest to finest, return the first level that has at
    // least target_peaks-per-channel.
    for (std::size_t i = levels_.size(); i > 0; --i) {
        const std::size_t idx = i - 1;
        if (levels_[idx].peaks_per_channel >= target_peaks) return idx;
    }
    return 0;  // finest available
}

std::size_t AudioThumbnail::render_min_max(uint32_t channel,
                                            uint32_t target_peaks,
                                            float* dst) const noexcept {
    if (dst == nullptr || target_peaks == 0 || levels_.empty()) {
        return 0;
    }

    const std::size_t lvl_idx = best_level_for(target_peaks);
    const ThumbnailLevel& lvl = levels_[lvl_idx];
    if (lvl.peaks_per_channel == 0) return 0;

    const std::size_t channels = lvl.peaks.size();
    const bool fold_all = (channel == kAllChannels) || (channel >= channels);

    const std::size_t src_count = lvl.peaks_per_channel;
    const std::size_t want = target_peaks;
    for (std::size_t p = 0; p < want; ++p) {
        // Map output index p ∈ [0, want) to a span over the source
        // [a, b) ⊂ [0, src_count) so we never sample past the end.
        const std::size_t a = (p * src_count) / want;
        std::size_t b = ((p + 1) * src_count) / want;
        if (b <= a) b = a + 1;
        if (b > src_count) b = src_count;

        float lo =  std::numeric_limits<float>::infinity();
        float hi = -std::numeric_limits<float>::infinity();

        if (fold_all) {
            for (std::size_t i = a; i < b; ++i) {
                for (std::size_t ch = 0; ch < channels; ++ch) {
                    const AudioPeak& pk = lvl.peaks[ch][i];
                    const float l = pk.min();
                    const float h = pk.max();
                    if (l < lo) lo = l;
                    if (h > hi) hi = h;
                }
            }
        } else {
            const auto& src = lvl.peaks[channel];
            for (std::size_t i = a; i < b; ++i) {
                const AudioPeak& pk = src[i];
                const float l = pk.min();
                const float h = pk.max();
                if (l < lo) lo = l;
                if (h > hi) hi = h;
            }
        }

        if (!std::isfinite(lo)) lo = 0.0f;
        if (!std::isfinite(hi)) hi = 0.0f;
        dst[p * 2 + 0] = lo;
        dst[p * 2 + 1] = hi;
    }
    return want;
}

// ─────────────────────────────────────────────────────────────────────────
// AudioThumbnailCache
// ─────────────────────────────────────────────────────────────────────────

AudioThumbnailCache::AudioThumbnailCache(std::size_t max_entries)
    : capacity_(max_entries == 0 ? 1u : max_entries) {}

void AudioThumbnailCache::touch_locked(Map::iterator it) {
    list_.splice(list_.begin(), list_, it->second);
}

void AudioThumbnailCache::evict_to_capacity_locked() {
    while (list_.size() > capacity_) {
        const auto& victim = list_.back();
        index_.erase(victim.key);
        list_.pop_back();
        evictions_.fetch_add(1, std::memory_order_relaxed);
    }
}

std::shared_ptr<const AudioThumbnail> AudioThumbnailCache::get(
    const std::string& path) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = index_.find(path);
    if (it == index_.end()) {
        misses_.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }
    touch_locked(it);
    hits_.fetch_add(1, std::memory_order_relaxed);
    return it->second->value;
}

void AudioThumbnailCache::put(const std::string& path,
                               std::shared_ptr<const AudioThumbnail> thumbnail) {
    if (!thumbnail) return;
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = index_.find(path);
    if (it != index_.end()) {
        it->second->value = std::move(thumbnail);
        touch_locked(it);
        return;
    }
    list_.push_front(Entry{path, std::move(thumbnail)});
    index_[path] = list_.begin();
    evict_to_capacity_locked();
}

std::shared_ptr<const AudioThumbnail> AudioThumbnailCache::get_or_build(
    const std::string& path,
    uint32_t samples_per_peak) {
    if (auto cached = get(path)) return cached;
    auto built = AudioThumbnail::build_from_path(path, samples_per_peak);
    if (!built) return nullptr;
    auto shared = std::make_shared<const AudioThumbnail>(std::move(*built));
    put(path, shared);
    return shared;
}

void AudioThumbnailCache::clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    list_.clear();
    index_.clear();
}

bool AudioThumbnailCache::erase(const std::string& path) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = index_.find(path);
    if (it == index_.end()) return false;
    list_.erase(it->second);
    index_.erase(it);
    return true;
}

AudioThumbnailCacheStats AudioThumbnailCache::stats() const {
    AudioThumbnailCacheStats s;
    s.hits = hits_.load(std::memory_order_relaxed);
    s.misses = misses_.load(std::memory_order_relaxed);
    s.evictions = evictions_.load(std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(mtx_);
        s.entries = list_.size();
    }
    s.capacity = capacity_;
    return s;
}

std::size_t AudioThumbnailCache::size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return list_.size();
}

void AudioThumbnailCache::set_capacity(std::size_t max_entries) {
    std::lock_guard<std::mutex> lock(mtx_);
    capacity_ = max_entries == 0 ? 1u : max_entries;
    evict_to_capacity_locked();
}

}  // namespace pulp::audio
