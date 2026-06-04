#pragma once

#include <pulp/scene/sidecar.hpp>

#include <vector>

namespace pulp::scene {

struct BakePreflightOptions {
    bool require_runtime_evidence = false;
    bool require_runtime_evidence_url = false;
};

struct BakePreflightReport {
    bool export_blocked = false;
    bool texture_encoding_blocked = false;
    bool native_runtime_has_gaps = false;
    bool has_error_diagnostics = false;
    bool runtime_evidence_missing = false;
    bool runtime_evidence_url_invalid = false;
    std::vector<UnsupportedFeature> export_blockers;
    std::vector<UnsupportedFeature> texture_encoding_blockers;
    std::vector<UnsupportedFeature> native_runtime_gaps;
    std::vector<Diagnostic> diagnostics;
};

BakePreflightReport analyze_bake_preflight(
    const SidecarData& sidecar,
    const BakePreflightOptions& options = {});
const char* bake_preflight_readiness(const BakePreflightReport& report);

} // namespace pulp::scene
