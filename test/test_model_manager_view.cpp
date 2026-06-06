// ModelManagerView (MM-PR3) — builds rows from a model list, shows a download-progress
// row, fires the action callbacks, and renders non-blank on the Skia canvas.
#include <catch2/catch_test_macros.hpp>

#include <pulp/runtime/model_store.hpp>
#include <pulp/view/model_manager_view.hpp>
#include <pulp/view/screenshot.hpp>

#include <fstream>
#include <string>

using namespace pulp::view;
using pulp::runtime::ListedModel;
using pulp::runtime::ModelListResult;

namespace {

ModelListResult make_models() {
    ModelListResult r;
    r.active_model_id = "mrt2_base";

    ListedModel a;
    a.model.model_id = "mrt2_base";
    a.model.display_name = "Magenta RT2 (Large)";
    a.model.is_recommended = true;
    a.status = "installed";
    a.active = true;

    ListedModel b;
    b.model.model_id = "mrt2_small";
    b.model.display_name = "Magenta RT2 (Small)";
    b.status = "not_installed";

    r.models = {a, b};
    return r;
}

View* last_child(View& v) { return v.child_at(v.child_count() - 1); }

}  // namespace

TEST_CASE("ModelManagerView builds a row per model", "[view][model]") {
    ModelManagerView v;
    v.set_models(make_models());
    REQUIRE(v.child_count() >= 3);          // title + subtitle + rows container
    REQUIRE(last_child(v)->child_count() == 2);  // two model rows
}

TEST_CASE("ModelManagerView shows a download-progress row", "[view][model]") {
    ModelManagerView v;
    v.set_models(make_models());

    auto* rows = last_child(v);
    const size_t before = rows->child_at(1)->child_count();  // mrt2_small idle row

    v.set_download_progress("mrt2_small", 0.47f);
    rows = last_child(v);  // rebuilt
    REQUIRE(rows->child_count() == 2);
    // Downloading row gains a meter + "%" + Cancel, so it has more children than idle.
    REQUIRE(rows->child_at(1)->child_count() > before);
}

TEST_CASE("ModelManagerView action callbacks fire with the model id", "[view][model]") {
    ModelManagerView v;
    std::string downloaded;
    std::string removed;
    v.on_download = [&](const std::string& id) { downloaded = id; };
    v.on_remove = [&](const std::string& id) { removed = id; };
    v.set_models(make_models());

    auto* rows = last_child(v);
    auto fire_buttons = [](View* row) {
        for (size_t i = 0; i < row->child_count(); ++i)
            if (auto* b = dynamic_cast<ToggleButton*>(row->child_at(i)))
                if (b->on_toggle) b->on_toggle(true);
    };
    fire_buttons(rows->child_at(0));  // mrt2_base installed+active → Remove
    fire_buttons(rows->child_at(1));  // mrt2_small not installed → Download

    REQUIRE(downloaded == "mrt2_small");
    REQUIRE(removed == "mrt2_base");
}

TEST_CASE("ModelManagerView renders non-blank on the Skia canvas", "[view][model][.demo]") {
    ModelManagerView v;
    v.set_models(make_models());
    v.set_download_progress("mrt2_small", 0.47f);

    auto png = render_to_png(v, 560, 280, 2.0f, ScreenshotBackend::skia);
    REQUIRE_FALSE(png.empty());
    std::ofstream("/tmp/model-manager.png", std::ios::binary)
        .write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
}
