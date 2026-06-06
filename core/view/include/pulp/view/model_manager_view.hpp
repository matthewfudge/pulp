#pragma once

// ModelManagerView (MM-PR3) — the "you need a model" manager UI. Lists available +
// installed models with their status, shows per-row download progress + cancel, and
// exposes Download / Set-default / Remove actions via callbacks the host wires to
// runtime::install_model / activate_model / remove_model. Pure composition over Pulp
// widgets (View flex + Label + Meter + ToggleButton) so it renders on the Skia/GPU
// canvas and is headless-capturable for visual regression.

#include <pulp/view/view.hpp>
#include <pulp/view/widgets.hpp>
#include <pulp/runtime/model_store.hpp>
#include <pulp/canvas/canvas.hpp>

#include <functional>
#include <map>
#include <string>

namespace pulp::view {

class ModelManagerView : public View {
public:
    using ModelAction = std::function<void(const std::string& model_id)>;

    ModelManagerView() {
        flex().direction = FlexDirection::column;
        flex().padding = 20.0f;
        flex().gap = 12.0f;
        set_background_color(canvas::Color::rgba8(32, 33, 36, 255));  // GREY_900-ish

        auto title = std::make_unique<Label>("Models");
        title->set_font_size(20.0f);
        title->set_font_weight(700);
        title->set_text_color(canvas::Color::rgba8(255, 255, 255, 255));
        title->flex().preferred_height = 28.0f;
        add_child(std::move(title));

        auto subtitle = std::make_unique<Label>(
            "Download a model to enable generation. Models are stored once and shared across plugins.");
        subtitle->set_font_size(12.0f);
        subtitle->set_text_color(canvas::Color::rgba8(170, 170, 175, 255));
        subtitle->flex().preferred_height = 18.0f;
        add_child(std::move(subtitle));

        rebuild();
    }

    /// Populate from runtime::list_models(registry, subsystem).
    void set_models(const runtime::ModelListResult& models) {
        models_ = models;
        rebuild();
    }

    /// Per-row download progress in [0,1]. Negative clears (not downloading).
    void set_download_progress(const std::string& model_id, float fraction) {
        if (fraction < 0.0f)
            progress_.erase(model_id);
        else
            progress_[model_id] = fraction;
        rebuild();
    }

    ModelAction on_download;          ///< user tapped Download / Resume
    ModelAction on_activate;          ///< user tapped Set default
    ModelAction on_remove;            ///< user tapped Remove / Delete
    ModelAction on_cancel;            ///< user tapped Cancel (mid-download)
    std::function<void()> on_done;    ///< user tapped Done (return to the editor)

private:
    static std::unique_ptr<Label> make_label(const std::string& text, float size,
                                              canvas::Color color, float width = 0.0f) {
        auto l = std::make_unique<Label>(text);
        l->set_font_size(size);
        l->set_text_color(color);
        if (width > 0.0f) l->flex().preferred_width = width;
        return l;
    }

    std::unique_ptr<ToggleButton> make_action(const std::string& label, const std::string& id,
                                              ModelAction ModelManagerView::* member) {
        auto b = std::make_unique<ToggleButton>();
        b->set_label(label);
        b->flex().preferred_width = 96.0f;
        b->flex().preferred_height = 28.0f;
        b->on_toggle = [this, id, member](bool) {
            if (this->*member) (this->*member)(id);
        };
        return b;
    }

    // A plain download progress bar: a dark track with a constant-accent fill sized to
    // the fraction. (Deliberately NOT the audio Meter widget — that colors by level
    // green→red, which reads as clipping, not progress.)
    static std::unique_ptr<View> make_progress_bar(float fraction, float width) {
        const float f = fraction < 0.0f ? 0.0f : (fraction > 1.0f ? 1.0f : fraction);
        auto track = std::make_unique<View>();
        track->flex().direction = FlexDirection::row;
        track->flex().preferred_width = width;
        track->flex().preferred_height = 8.0f;
        track->set_background_color(canvas::Color::rgba8(55, 56, 60, 255));
        auto fill = std::make_unique<View>();
        fill->flex().preferred_width = f * width < 2.0f ? 2.0f : f * width;
        fill->flex().preferred_height = 8.0f;
        fill->set_background_color(canvas::Color::rgba8(127, 178, 255, 255));
        track->add_child(std::move(fill));
        return track;
    }

    void rebuild() {
        if (rows_) {
            remove_child(rows_);
            rows_ = nullptr;
        }
        auto rows = std::make_unique<View>();
        rows->flex().direction = FlexDirection::column;
        rows->flex().gap = 8.0f;
        rows->flex().flex_grow = 1.0f;

        const auto muted = canvas::Color::rgba8(170, 170, 175, 255);
        const auto white = canvas::Color::rgba8(235, 235, 240, 255);
        const auto teal = canvas::Color::rgba8(132, 243, 237, 255);
        const auto blue = canvas::Color::rgba8(127, 178, 255, 255);

        // When a model is active there's an editor to return to — offer a way back.
        bool has_active = false;
        for (const auto& l : models_.models)
            if (l.active) { has_active = true; break; }
        if (has_active && on_done) {
            auto done = std::make_unique<ToggleButton>();
            done->set_label("← Done");
            done->flex().preferred_width = 96.0f;
            done->flex().preferred_height = 28.0f;
            done->on_toggle = [this](bool) { if (on_done) on_done(); };
            rows->add_child(std::move(done));
        }

        if (models_.models.empty()) {
            auto empty = make_label("No models available yet.", 14.0f, muted);
            rows->add_child(std::move(empty));
        }

        for (const auto& listed : models_.models) {
            const std::string id = listed.model.model_id;

            auto row = std::make_unique<View>();
            row->flex().direction = FlexDirection::row;
            row->flex().gap = 10.0f;
            row->flex().preferred_height = 40.0f;
            row->flex().align_items = FlexAlign::center;

            std::string name = listed.model.display_name.empty() ? id : listed.model.display_name;
            if (listed.model.is_recommended) name = "★ " + name;  // ★
            auto name_label = make_label(name, 14.0f, white);
            name_label->flex().flex_grow = 1.0f;
            row->add_child(std::move(name_label));

            const auto it = progress_.find(id);
            if (it != progress_.end()) {  // actively downloading
                const int pct = static_cast<int>(it->second * 100.0f + 0.5f);
                row->add_child(make_progress_bar(it->second, 140.0f));
                row->add_child(make_label(std::to_string(pct) + "%", 12.0f, teal, 44.0f));
                row->add_child(make_action("Cancel", id, &ModelManagerView::on_cancel));
            } else if (listed.status == "partial") {  // paused / cancelled — resumable
                const float frac = listed.partial_fraction < 0.0f ? 0.0f : listed.partial_fraction;
                row->add_child(make_progress_bar(frac, 140.0f));
                row->add_child(make_label("Paused " + std::to_string(static_cast<int>(frac * 100.0f + 0.5f)) + "%",
                                          12.0f, muted, 90.0f));
                row->add_child(make_action("Resume", id, &ModelManagerView::on_download));
                row->add_child(make_action("Delete", id, &ModelManagerView::on_remove));
            } else if (listed.status == "installed") {
                row->add_child(make_label(listed.active ? "Active" : "Installed", 12.0f,
                                          listed.active ? teal : blue, 80.0f));
                if (!listed.active)
                    row->add_child(make_action("Set default", id, &ModelManagerView::on_activate));
                row->add_child(make_action("Remove", id, &ModelManagerView::on_remove));
            } else {  // available
                row->add_child(make_label("Available", 12.0f, muted, 80.0f));
                row->add_child(make_action("Download", id, &ModelManagerView::on_download));
            }
            rows->add_child(std::move(row));
        }

        rows_ = rows.get();
        add_child(std::move(rows));
    }

    runtime::ModelListResult models_;
    std::map<std::string, float> progress_;
    View* rows_ = nullptr;
};

}  // namespace pulp::view
