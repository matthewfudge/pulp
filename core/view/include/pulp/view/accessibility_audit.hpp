#pragma once

// Accessibility audit (workstream 04 slice 4.6).
//
// Walks a Pulp view tree and reports issues that would make the plugin
// unusable with a screen reader. Consumed by `pulp doctor --accessibility`
// and by unit tests that want to fail on missing labels. Pure function —
// no platform dependencies, so the same audit runs on every OS.

#include <pulp/view/view.hpp>

#include <string>
#include <vector>

namespace pulp::view {

enum class AccessibilityIssueKind {
    MissingLabel,            ///< role != none but access_label is empty
    SuspiciousNoneRole,      ///< looks interactive (focusable/on_tap...) but role is none
    ZeroSize,                ///< width or height is 0 — element is invisible
    DuplicateLabel,          ///< same (role, label) appears more than once under root
};

struct AccessibilityIssue {
    const View* view = nullptr;
    AccessibilityIssueKind kind;
    std::string detail;       ///< human-readable explanation
    int depth = 0;            ///< where in the tree the offender lives
};

/// Walk `root` depth-first and return every audit failure found. Empty
/// vector = clean audit. Callers (doctor, tests) format these into a
/// human report.
std::vector<AccessibilityIssue> audit_accessibility(const View& root);

/// Stable string tag for logging / matrices.
std::string_view to_string(AccessibilityIssueKind kind);

}  // namespace pulp::view
