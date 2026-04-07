#include <catch2/catch_test_macros.hpp>
#include <pulp/runtime/i18n.hpp>
#include <pulp/runtime/temporary_file.hpp>
#include <fstream>

using namespace pulp::runtime;

// ── In-memory operations ────────────────────────────────────────────────

TEST_CASE("i18n add and translate", "[runtime][i18n]") {
    LocalisedStrings strings;
    strings.add("greeting", "Hello");
    REQUIRE(strings.translate("greeting") == "Hello");
    REQUIRE(strings.has("greeting"));
    REQUIRE(strings.count() == 1);
}

TEST_CASE("i18n translate missing key returns key", "[runtime][i18n]") {
    LocalisedStrings strings;
    REQUIRE(strings.translate("missing_key") == "missing_key");
}

TEST_CASE("i18n argument substitution", "[runtime][i18n]") {
    LocalisedStrings strings;
    strings.add("welcome", "Hello {0}, you have {1} messages");
    auto result = strings.translate("welcome", {"Alice", "5"});
    REQUIRE(result == "Hello Alice, you have 5 messages");
}

TEST_CASE("i18n argument substitution with multiple occurrences", "[runtime][i18n]") {
    LocalisedStrings strings;
    strings.add("repeat", "{0} and {0}");
    REQUIRE(strings.translate("repeat", {"yes"}) == "yes and yes");
}

TEST_CASE("i18n clear removes all translations", "[runtime][i18n]") {
    LocalisedStrings strings;
    strings.add("a", "1");
    strings.add("b", "2");
    REQUIRE(strings.count() == 2);
    strings.clear();
    REQUIRE(strings.count() == 0);
}

TEST_CASE("i18n locale get and set", "[runtime][i18n]") {
    LocalisedStrings strings;
    REQUIRE(strings.locale() == "en");
    strings.set_locale("de");
    REQUIRE(strings.locale() == "de");
}

// ── .strings file format ─────────────────────────────────────────────────

TEST_CASE("i18n load .strings file", "[runtime][i18n]") {
    TemporaryFile tmp(".strings");
    {
        std::ofstream f(tmp.path());
        f << "/* Comment */\n";
        f << "\"cancel\" = \"Cancel\";\n";
        f << "\"ok\" = \"OK\";\n";
        f << "\"save_as\" = \"Save As...\";\n";
    }

    LocalisedStrings strings;
    REQUIRE(strings.load_strings_file(tmp.path_string()));
    REQUIRE(strings.count() == 3);
    REQUIRE(strings.translate("cancel") == "Cancel");
    REQUIRE(strings.translate("ok") == "OK");
    REQUIRE(strings.translate("save_as") == "Save As...");
}

// ── .po file format ─────────────────────────────────────────────────────

TEST_CASE("i18n load .po file", "[runtime][i18n]") {
    TemporaryFile tmp(".po");
    {
        std::ofstream f(tmp.path());
        f << "# Translation file\n";
        f << "msgid \"hello\"\n";
        f << "msgstr \"Hallo\"\n";
        f << "\n";
        f << "msgid \"goodbye\"\n";
        f << "msgstr \"Auf Wiedersehen\"\n";
    }

    LocalisedStrings strings;
    REQUIRE(strings.load_po_file(tmp.path_string()));
    REQUIRE(strings.count() == 2);
    REQUIRE(strings.translate("hello") == "Hallo");
    REQUIRE(strings.translate("goodbye") == "Auf Wiedersehen");
}

// ── JSON file format ────────────────────────────────────────────────────

TEST_CASE("i18n load JSON file", "[runtime][i18n]") {
    TemporaryFile tmp(".json");
    {
        std::ofstream f(tmp.path());
        f << "{\n";
        f << "  \"title\": \"Pulp Audio\",\n";
        f << "  \"version_label\": \"Version {0}\",\n";
        f << "  \"empty\": \"\"\n";
        f << "}\n";
    }

    LocalisedStrings strings;
    REQUIRE(strings.load_json_file(tmp.path_string()));
    REQUIRE(strings.count() == 3);
    REQUIRE(strings.translate("title") == "Pulp Audio");
    REQUIRE(strings.translate("version_label", {"1.0"}) == "Version 1.0");
    REQUIRE(strings.translate("empty") == "");
}

// ── File load failures ──────────────────────────────────────────────────

TEST_CASE("i18n load nonexistent file returns false", "[runtime][i18n]") {
    LocalisedStrings strings;
    REQUIRE_FALSE(strings.load_strings_file("/tmp/nonexistent_i18n_12345.strings"));
    REQUIRE_FALSE(strings.load_po_file("/tmp/nonexistent_i18n_12345.po"));
    REQUIRE_FALSE(strings.load_json_file("/tmp/nonexistent_i18n_12345.json"));
}

// ── Global instance ─────────────────────────────────────────────────────

TEST_CASE("i18n global instance", "[runtime][i18n]") {
    auto& inst = LocalisedStrings::instance();
    inst.add("test_global", "works");
    REQUIRE(tr("test_global") == "works");
    inst.clear();
}

// ── System locale detection ─────────────────────────────────────────────

TEST_CASE("i18n system_locale returns non-empty", "[runtime][i18n]") {
    auto locale = LocalisedStrings::system_locale();
    REQUIRE_FALSE(locale.empty());
}
