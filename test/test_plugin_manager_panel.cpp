// Headless tests for `pulp::view::PluginManagerPanel` (issue #494).
//
// Drives the widget against an in-memory `InMemoryPluginManagerModel` so
// the test is deterministic and has no filesystem or subprocess
// dependencies. Covers the acceptance criteria in #494: three buckets,
// search filter, rescan button, context menu, blacklist persistence
// round-trip, and accessibility labels.

#include <catch2/catch_test_macros.hpp>

#include <pulp/host/scan_blacklist.hpp>
#include <pulp/host/scanner.hpp>
#include <pulp/view/input_events.hpp>
#include <pulp/view/plugin_manager_panel.hpp>

using namespace pulp::view;
using pulp::host::PluginFormat;

namespace {

PluginManagerRow make_row(PluginFormat fmt, std::string name, std::string path,
                          std::int64_t scan_time = 1700000000)
{
    PluginManagerRow r;
    r.format = fmt;
    r.name = std::move(name);
    r.path = std::move(path);
    r.last_scan_unix = scan_time;
    return r;
}

void populate_model(InMemoryPluginManagerModel& m) {
    m.scanned_rows = {
        make_row(PluginFormat::CLAP, "Solid Bass",      "/plugins/SolidBass.clap"),
        make_row(PluginFormat::VST3, "Glacier Reverb",  "/plugins/Glacier.vst3"),
        make_row(PluginFormat::AudioUnit, "Airy Chorus",
                 "/Library/Audio/Plug-Ins/Components/Airy.component"),
    };
    PluginManagerRow failed = make_row(PluginFormat::CLAP, "Broken Synth",
                                       "/plugins/Broken.clap");
    failed.reason = "SIGSEGV during scan";
    m.failed_rows = {failed};
}

} // namespace

TEST_CASE("PluginManagerPanel populates each bucket from the model",
          "[view][plugin_manager][issue-494]")
{
    InMemoryPluginManagerModel model; populate_model(model);
    PluginManagerPanel panel(model);
    panel.set_bounds({0, 0, 900, 400});

    REQUIRE(panel.visible_count(PluginManagerBucket::scanned) == 3);
    REQUIRE(panel.visible_count(PluginManagerBucket::failed) == 1);
    REQUIRE(panel.visible_count(PluginManagerBucket::blacklisted) == 0);

    const auto& scanned = panel.rows(PluginManagerBucket::scanned);
    REQUIRE(scanned[0].name == "Solid Bass");
    REQUIRE(scanned[1].name == "Glacier Reverb");

    const auto& failed = panel.rows(PluginManagerBucket::failed);
    REQUIRE(failed[0].reason == "SIGSEGV during scan");
}

TEST_CASE("PluginManagerPanel search filter narrows all buckets",
          "[view][plugin_manager][issue-494]")
{
    InMemoryPluginManagerModel model; populate_model(model);
    PluginManagerPanel panel(model);

    panel.set_filter("reverb");
    REQUIRE(panel.visible_count(PluginManagerBucket::scanned) == 1);
    REQUIRE(panel.visible_count(PluginManagerBucket::failed) == 0);

    panel.set_filter("broken");
    REQUIRE(panel.visible_count(PluginManagerBucket::scanned) == 0);
    REQUIRE(panel.visible_count(PluginManagerBucket::failed) == 1);

    // Filter matches against the path too.
    panel.set_filter("/Library/Audio");
    REQUIRE(panel.visible_count(PluginManagerBucket::scanned) == 1);

    panel.set_filter("");
    REQUIRE(panel.visible_count(PluginManagerBucket::scanned) == 3);
}

TEST_CASE("PluginManagerPanel rescan button drives the model non-blockingly",
          "[view][plugin_manager][issue-494]")
{
    InMemoryPluginManagerModel model; populate_model(model);
    PluginManagerPanel panel(model);

    REQUIRE(model.rescan_count == 0);
    panel.trigger_rescan();
    REQUIRE(model.rescan_count == 1);

    model.scanning = true;
    model.progress = 0.42f;
    REQUIRE(panel.model().is_scanning());
    REQUIRE(panel.model().progress_fraction() == 0.42f);
}

TEST_CASE("PluginManagerPanel context menu drives per-row actions",
          "[view][plugin_manager][issue-494]")
{
    InMemoryPluginManagerModel model; populate_model(model);
    PluginManagerPanel panel(model);

    const std::string bass_path = "/plugins/SolidBass.clap";
    panel.open_context_menu(PluginManagerBucket::scanned, bass_path);
    REQUIRE(panel.context_menu_path() == bass_path);

    SECTION("rescan just this plugin") {
        panel.activate_context_item(PluginManagerPanel::ContextItem::rescan_this);
        REQUIRE(model.single_rescan_count == 1);
        REQUIRE(model.last_single_rescan_path == bass_path);
    }

    SECTION("toggle blacklist moves the row to the blacklisted bucket") {
        REQUIRE_FALSE(model.is_blacklisted(bass_path));
        panel.activate_context_item(PluginManagerPanel::ContextItem::toggle_blacklist);
        REQUIRE(model.is_blacklisted(bass_path));
        REQUIRE(panel.visible_count(PluginManagerBucket::scanned) == 2);
        REQUIRE(panel.visible_count(PluginManagerBucket::blacklisted) == 1);

        // The label reflects the current state so the second click
        // re-exposes the plugin.
        panel.open_context_menu(PluginManagerBucket::blacklisted, bass_path);
        REQUIRE(panel.context_menu_label(
            PluginManagerPanel::ContextItem::toggle_blacklist) ==
            "Remove from blacklist");
        panel.activate_context_item(PluginManagerPanel::ContextItem::toggle_blacklist);
        REQUIRE_FALSE(model.is_blacklisted(bass_path));
    }

    SECTION("reveal in file manager routes through the model") {
        panel.activate_context_item(
            PluginManagerPanel::ContextItem::reveal_in_file_manager);
        REQUIRE(model.reveal_count == 1);
        REQUIRE(model.last_reveal_path == bass_path);
    }
}

TEST_CASE("PluginManagerPanel per-format search paths round-trip via the model",
          "[view][plugin_manager][issue-494]")
{
    InMemoryPluginManagerModel model;
    PluginManagerPanel panel(model);

    panel.add_search_path(PluginFormat::VST3, "/Library/Audio/Plug-Ins/VST3");
    panel.add_search_path(PluginFormat::VST3, "/Users/me/VST3");
    panel.add_search_path(PluginFormat::CLAP, "/Library/Audio/Plug-Ins/CLAP");

    REQUIRE(panel.search_paths(PluginFormat::VST3).size() == 2);
    REQUIRE(panel.search_paths(PluginFormat::CLAP).size() == 1);

    // De-dup on repeat add.
    panel.add_search_path(PluginFormat::VST3, "/Library/Audio/Plug-Ins/VST3");
    REQUIRE(panel.search_paths(PluginFormat::VST3).size() == 2);

    panel.remove_search_path(PluginFormat::VST3, "/Users/me/VST3");
    REQUIRE(panel.search_paths(PluginFormat::VST3).size() == 1);
    REQUIRE(panel.search_paths(PluginFormat::VST3)[0] ==
            "/Library/Audio/Plug-Ins/VST3");
}

TEST_CASE("PluginManagerPanel blacklist persists via ScanBlacklist "
          "text round-trip",
          "[view][plugin_manager][issue-494]")
{
    InMemoryPluginManagerModel model; populate_model(model);
    PluginManagerPanel panel(model);

    panel.toggle_blacklist("/plugins/SolidBass.clap");
    panel.toggle_blacklist("/plugins/Broken.clap");

    // Serialize the underlying blacklist to the on-disk format.
    const std::string serialized = model.blacklist.to_text();
    REQUIRE(!serialized.empty());

    // Fresh model — imagine a new session — can re-hydrate from the same
    // text blob.
    InMemoryPluginManagerModel next;
    REQUIRE(next.blacklist.from_text(serialized));

    PluginManagerPanel next_panel(next);
    REQUIRE(next.is_blacklisted("/plugins/SolidBass.clap"));
    REQUIRE(next.is_blacklisted("/plugins/Broken.clap"));
    REQUIRE(next_panel.visible_count(PluginManagerBucket::blacklisted) == 2);
}

TEST_CASE("PluginManagerPanel exposes screen-reader labels on each column",
          "[view][plugin_manager][issue-494]")
{
    InMemoryPluginManagerModel model; populate_model(model);
    PluginManagerPanel panel(model);

    REQUIRE(panel.access_role() == pulp::view::View::AccessRole::group);
    REQUIRE(panel.access_label() == "Plugin manager");

    REQUIRE(panel.column_access_label(PluginManagerBucket::scanned)
              == "Scanned plugins — 3 entries");
    REQUIRE(panel.column_access_label(PluginManagerBucket::failed)
              == "Failed plugins — 1 entry");
    REQUIRE(panel.column_access_label(PluginManagerBucket::blacklisted)
              == "Blacklisted plugins — 0 entries");
}

TEST_CASE("PluginManagerPanel text input appends to the filter "
          "and backspace removes characters",
          "[view][plugin_manager][issue-494]")
{
    InMemoryPluginManagerModel model; populate_model(model);
    PluginManagerPanel panel(model);

    TextInputEvent te;
    te.text = "gla";
    panel.on_text_input(te);
    REQUIRE(panel.filter() == "gla");
    REQUIRE(panel.visible_count(PluginManagerBucket::scanned) == 1);

    KeyEvent ke;
    ke.key = KeyCode::backspace;
    ke.is_down = true;
    REQUIRE(panel.on_key_event(ke));
    REQUIRE(panel.filter() == "gl");
}
