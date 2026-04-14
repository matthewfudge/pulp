#include <pulp/view/accessibility_audit.hpp>

#include <unordered_map>

namespace pulp::view {

namespace {

struct LabelKey {
    View::AccessRole role;
    std::string label;
    bool operator==(const LabelKey& o) const {
        return role == o.role && label == o.label;
    }
};

struct LabelKeyHash {
    std::size_t operator()(const LabelKey& k) const noexcept {
        return std::hash<int>()(static_cast<int>(k.role)) ^
               (std::hash<std::string>()(k.label) << 1);
    }
};

void walk(const View& v, int depth,
          std::vector<AccessibilityIssue>& out,
          std::unordered_map<LabelKey, const View*, LabelKeyHash>& seen) {
    const auto role = v.access_role();
    const auto& label = v.access_label();

    // Issue: role != none but no label.
    if (role != View::AccessRole::none && label.empty()) {
        out.push_back({&v,
                       AccessibilityIssueKind::MissingLabel,
                       "view has an accessibility role but no label",
                       depth});
    }

    // Issue: zero width/height — invisible to sighted users and
    // unreachable via hit-testing.
    auto b = v.bounds();
    if (b.width == 0.0f || b.height == 0.0f) {
        out.push_back({&v,
                       AccessibilityIssueKind::ZeroSize,
                       "view has zero width or height",
                       depth});
    }

    // Issue: duplicate (role, label) — screen readers announce both
    // identically, which confuses the user.
    if (role != View::AccessRole::none && !label.empty()) {
        LabelKey key{role, label};
        auto [it, inserted] = seen.emplace(key, &v);
        if (!inserted) {
            out.push_back({&v,
                           AccessibilityIssueKind::DuplicateLabel,
                           "another view already uses the same "
                           "(role, label) — announcements will collide",
                           depth});
        }
    }

    for (std::size_t i = 0; i < v.child_count(); ++i) {
        if (const View* child = v.child_at(i)) {
            walk(*child, depth + 1, out, seen);
        }
    }
}

} // namespace

std::vector<AccessibilityIssue> audit_accessibility(const View& root) {
    std::vector<AccessibilityIssue> out;
    std::unordered_map<LabelKey, const View*, LabelKeyHash> seen;
    walk(root, 0, out, seen);
    return out;
}

std::string_view to_string(AccessibilityIssueKind kind) {
    switch (kind) {
        case AccessibilityIssueKind::MissingLabel:       return "missing-label";
        case AccessibilityIssueKind::SuspiciousNoneRole: return "suspicious-none-role";
        case AccessibilityIssueKind::ZeroSize:           return "zero-size";
        case AccessibilityIssueKind::DuplicateLabel:     return "duplicate-label";
    }
    return "unknown";
}

} // namespace pulp::view
