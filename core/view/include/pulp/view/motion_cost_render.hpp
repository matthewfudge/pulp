#pragma once

/// @file motion_cost_render.hpp
/// Helpers that build a `CostProbe` from Pulp's render-layer
/// subsystems. Kept in a separate header so `motion_cost.hpp`
/// consumers don't drag in `pulp/render/*` transitively — only
/// callers that actually have a RenderPassManager / DirtyTracker
/// instance to wire need this.
///
/// Both subsystems are intentionally thin today (RenderPassManager
/// tracks total_time_ms per frame; DirtyTracker tracks rect lists).
/// The helpers below snapshot exactly what those types expose and
/// nothing more — they explicitly do NOT measure frame duration on
/// their own, since the render layer's existing time_ms inputs come
/// from outside (caller of `end_pass(time_ms, ...)`).
///
/// When the render layer eventually grows a self-measured frame
/// duration (e.g. a FrameDurationProbe attached to RenderPassManager),
/// extend the helper here rather than growing RenderPassManager into
/// a profiler.

#include <pulp/render/dirty_tracker.hpp>
#include <pulp/render/render_pass.hpp>
#include <pulp/view/motion_cost.hpp>

namespace pulp::view::motion {

/// Build a CostProbe that snapshots both subsystems each tick.
/// Either pointer may be null; the corresponding fields default to
/// 0. Pointers must outlive the returned probe — callers typically
/// own these as members of their render host.
inline CostProbe make_render_cost_probe(
    const pulp::render::RenderPassManager* passes,
    const pulp::render::DirtyTracker* dirty
) {
    return [passes, dirty]() -> RenderCostSnapshot {
        RenderCostSnapshot s;
        if (passes) {
            s.render_pass_duration_ms =
                static_cast<double>(passes->total_time_ms());
        }
        if (dirty) {
            const auto& rects = dirty->dirty_rects();
            double area = 0.0;
            for (const auto& r : rects) {
                area += static_cast<double>(r.area());
            }
            s.dirty_rect_area_px = area;
            s.dirty_rect_count = static_cast<int>(rects.size());
        }
        return s;
    };
}

} // namespace pulp::view::motion
