// build_activation_view — a Pulp-native activation panel rendered entirely in
// the Pulp view tree (no WebView / OS overlay), so it composites through the
// Skia/canvas path and is capturable by Pulp's headless tooling.
//
// Every color resolves through pulp::view::Theme tokens (Ink & Signal naming):
// there is NO hard-coded Moonbase palette. The panel inherits the host
// plugin/app theme and is reskinnable via Theme / theme.json / token import.
// Moonbase branding lives in the hosted portal/browser flow, not here.

#pragma once

#include "moonbase_activation_controller.hpp"

#include <pulp/view/buttons.hpp>
#include <pulp/view/theme.hpp>
#include <pulp/view/view.hpp>
#include <pulp/view/widgets.hpp>

#include <memory>
#include <string>

namespace moonbase_pulp {

namespace detail {

// Root that paints the themed background and lays its children out by explicit
// bounds (suppressing the flex pass) for a deterministic, capturable panel.
class ActivationPanelRoot : public pulp::view::View {
public:
    void paint(pulp::canvas::Canvas& canvas) override
    {
        const auto bg = resolve_color("bg.primary", pulp::canvas::Color::rgba8(30, 30, 46));
        canvas.set_fill_color(bg);
        canvas.fill_rect(0, 0, bounds().width, bounds().height);
    }
    void layout_children() override {}
};

inline std::string title_for(MoonbaseActivationController::Screen screen)
{
    using S = MoonbaseActivationController::Screen;
    switch (screen) {
        case S::Loading:     return "Checking your license…";
        case S::Welcome:     return "Activate to unlock";
        case S::BrowserWait: return "Waiting for activation…";
        case S::Success:     return "You're all set";
        case S::Trial:       return "Trial active";
        case S::Details:     return "Licensed";
        case S::Expired:     return "Trial expired";
        case S::Error:       return "Something went wrong";
    }
    return "";
}

inline std::string body_for(const MoonbaseActivationController& controller)
{
    using S = MoonbaseActivationController::Screen;
    switch (controller.screen()) {
        case S::Loading:     return "Validating your stored license.";
        case S::Welcome:     return "Activate this device to unlock the full version.";
        case S::BrowserWait: return "Finish activating in the browser window we opened.";
        case S::Success:     return "This device is now activated.";
        case S::Trial:       return "You're using a trial. Activate to keep full access.";
        case S::Details:
            return controller.license()
                ? "Licensed to " + controller.license()->issued_to.name +
                      " (" + controller.license()->issued_to.email + ")."
                : "This device is activated.";
        case S::Expired:     return "Your trial has ended. Activate to continue.";
        case S::Error:       return controller.status_message().empty()
                                 ? "Please try again."
                                 : controller.status_message();
    }
    return "";
}

inline std::string action_label_for(MoonbaseActivationController::Screen screen)
{
    using S = MoonbaseActivationController::Screen;
    switch (screen) {
        case S::Welcome:
        case S::Expired:
        case S::Error:       return "Activate";
        case S::Trial:       return "Unlock full version";
        case S::BrowserWait: return "Cancel";
        case S::Success:
        case S::Details:     return "Deactivate this device";
        case S::Loading:     return "";
    }
    return "";
}

} // namespace detail

// Build the activation panel for the controller's current screen, themed via
// `theme`. Pure construction: callers wire the action button as needed.
inline std::unique_ptr<pulp::view::View> build_activation_view(
    const MoonbaseActivationController& controller,
    const pulp::view::Theme& theme,
    float width = 420.0f,
    float height = 280.0f)
{
    using pulp::view::Label;
    using pulp::view::TextButton;
    using Color = pulp::canvas::Color;

    const auto text_primary = theme.color("text.primary").value_or(Color::rgba8(205, 214, 244));
    const auto text_secondary = theme.color("text.secondary").value_or(Color::rgba8(166, 173, 200));

    auto root = std::make_unique<detail::ActivationPanelRoot>();
    root->set_theme(theme);
    root->set_bounds({0, 0, width, height});

    const float margin = 32.0f;
    const float content_w = width - 2 * margin;

    auto title = std::make_unique<Label>(detail::title_for(controller.screen()));
    title->set_font_size(24.0f);
    title->set_font_weight(700);
    title->set_text_color(text_primary);
    title->set_bounds({margin, 40.0f, content_w, 32.0f});
    root->add_child(std::move(title));

    auto body = std::make_unique<Label>(detail::body_for(controller));
    body->set_font_size(15.0f);
    body->set_multi_line(true);
    body->set_text_color(text_secondary);
    body->set_bounds({margin, 84.0f, content_w, 80.0f});
    root->add_child(std::move(body));

    const auto action = detail::action_label_for(controller.screen());
    if (!action.empty()) {
        auto button = std::make_unique<TextButton>(action);
        // Deactivate is a neutral/secondary action; everything else is the
        // primary call-to-action. Colors come from the theme's button styles.
        const bool destructive =
            controller.screen() == MoonbaseActivationController::Screen::Details ||
            controller.screen() == MoonbaseActivationController::Screen::Success ||
            controller.screen() == MoonbaseActivationController::Screen::BrowserWait;
        button->set_style(destructive ? TextButton::Style::secondary
                                      : TextButton::Style::primary);
        button->set_bounds({margin, height - 40.0f - 32.0f, 220.0f, 40.0f});
        root->add_child(std::move(button));
    }

    return root;
}

} // namespace moonbase_pulp
