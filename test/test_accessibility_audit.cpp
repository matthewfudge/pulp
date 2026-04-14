// Accessibility audit tests (workstream 04 slice 4.6).

#include <catch2/catch_test_macros.hpp>
#include <pulp/view/accessibility_audit.hpp>

using namespace pulp::view;

namespace {

class Probe : public View {};

std::unique_ptr<Probe> make(View::AccessRole role, std::string label,
                            float w = 100, float h = 20) {
    auto p = std::make_unique<Probe>();
    p->set_access_role(role);
    p->set_access_label(std::move(label));
    p->set_bounds({0, 0, w, h});
    return p;
}

} // namespace

TEST_CASE("clean tree produces no issues", "[a11y][audit]") {
    Probe root;
    root.set_bounds({0, 0, 200, 100});
    root.set_access_role(View::AccessRole::group);
    root.set_access_label("Root");
    root.add_child(make(View::AccessRole::slider, "Gain"));
    root.add_child(make(View::AccessRole::slider, "Mix"));
    root.add_child(make(View::AccessRole::label,  "Title"));
    REQUIRE(audit_accessibility(root).empty());
}

TEST_CASE("missing label on announceable view is flagged",
          "[a11y][audit]") {
    Probe root;
    root.set_bounds({0, 0, 200, 100});
    root.add_child(make(View::AccessRole::slider, "")); // no label

    auto issues = audit_accessibility(root);
    REQUIRE(issues.size() == 1);
    REQUIRE(issues[0].kind == AccessibilityIssueKind::MissingLabel);
}

TEST_CASE("zero-size view is flagged", "[a11y][audit]") {
    Probe root;
    root.set_bounds({0, 0, 200, 100});
    root.add_child(make(View::AccessRole::none, "", 0, 20));

    auto issues = audit_accessibility(root);
    REQUIRE(issues.size() == 1);
    REQUIRE(issues[0].kind == AccessibilityIssueKind::ZeroSize);
}

TEST_CASE("duplicate (role, label) is flagged on the second occurrence",
          "[a11y][audit]") {
    Probe root;
    root.set_bounds({0, 0, 200, 100});
    root.add_child(make(View::AccessRole::slider, "Gain"));
    root.add_child(make(View::AccessRole::slider, "Gain"));
    auto issues = audit_accessibility(root);
    REQUIRE(issues.size() == 1);
    REQUIRE(issues[0].kind == AccessibilityIssueKind::DuplicateLabel);
}

TEST_CASE("to_string covers every issue kind", "[a11y][audit]") {
    REQUIRE(to_string(AccessibilityIssueKind::MissingLabel) == "missing-label");
    REQUIRE(to_string(AccessibilityIssueKind::ZeroSize)     == "zero-size");
    REQUIRE(to_string(AccessibilityIssueKind::DuplicateLabel) == "duplicate-label");
    REQUIRE(to_string(AccessibilityIssueKind::SuspiciousNoneRole) == "suspicious-none-role");
}
