// design_import_designmd.cpp — DESIGN.md import adapter
//
// Inspired by Google's design.md (Apache-2.0, https://github.com/google/design.md).
// Reimplemented in C++; uses yaml-cpp (MIT) for frontmatter parsing. The
// Markdown body is walked with a small hand-rolled `##` section scanner —
// the body section structure is simple enough that a full Markdown parser
// would be overkill and would add an unnecessary dependency.
//
// What this file parses:
//   - YAML frontmatter (between `---` fences) → IR token maps via yaml-cpp.
//   - `## Section` headings in the Markdown body → ordered section list
//     (the Phase 2 section-order lint rule reads this).
//
// What this file does NOT do:
//   - Emit a UI tree. DESIGN.md describes a *system*, not a *screen*. The
//     IR root node is left empty (type "frame", no children). The Phase 1
//     CLI deliberately does not write a ui.js file when the source is
//     designmd; see tools/import-design/pulp_import_design.cpp for the
//     dispatch arm.

#include <pulp/view/design_import.hpp>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>

namespace pulp::view {

namespace {

// ── Frontmatter extraction ──────────────────────────────────────────────

struct FrontmatterSlice {
    bool present = false;
    std::string yaml_text;
    std::string body;       // markdown after the closing fence
    int yaml_start_line = 1;
};

// Split the input on the leading `---\n` / `\n---\n` fences. If the
// document does not begin with `---` then the whole input is the body.
FrontmatterSlice split_frontmatter(const std::string& markdown) {
    FrontmatterSlice out;
    if (markdown.size() < 4 || markdown.substr(0, 3) != "---" ||
        (markdown[3] != '\n' && markdown[3] != '\r')) {
        out.body = markdown;
        return out;
    }
    // Find the next `---` on its own line.
    size_t start = (markdown[3] == '\r' && markdown.size() > 4 && markdown[4] == '\n')
                       ? 5 : 4;
    std::regex closing(R"((?:^|\n)---[ \t]*(?:\r?\n|$))");
    std::smatch m;
    std::string tail(markdown.begin() + static_cast<std::ptrdiff_t>(start), markdown.end());
    if (!std::regex_search(tail, m, closing)) {
        out.body = markdown;
        return out;
    }
    out.present = true;
    out.yaml_text = tail.substr(0, m.position(0));
    out.yaml_start_line = 2; // frontmatter starts on line 2 (line 1 is the opening ---)
    auto body_start = m.position(0) + m.length(0);
    out.body = tail.substr(static_cast<size_t>(body_start));
    return out;
}

// ── Helpers ─────────────────────────────────────────────────────────────

std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Convert a dimension string ("48px", "1.5rem", "0.1em") to a float in px.
// Returns std::nullopt for values that aren't dimensions (e.g. plain strings).
std::optional<float> parse_dimension(const std::string& raw) {
    if (raw.empty()) return std::nullopt;
    static const std::regex re(R"(^\s*(-?\d+(?:\.\d+)?)\s*(px|em|rem)?\s*$)");
    std::smatch m;
    if (!std::regex_match(raw, m, re)) return std::nullopt;
    float value = std::stof(m[1]);
    std::string unit = m[2].matched ? m[2].str() : "px";
    if (unit == "em" || unit == "rem") value *= 16.0f; // canonical 1rem == 16px
    return value;
}

bool looks_like_hex_color(const std::string& s) {
    if (s.size() < 4 || s[0] != '#') return false;
    for (size_t i = 1; i < s.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return s.size() == 4 || s.size() == 7 || s.size() == 9;
}

bool is_token_reference(const std::string& s) {
    return s.size() >= 3 && s.front() == '{' && s.back() == '}';
}

std::string reference_path(const std::string& ref) {
    // "{colors.primary}" → "colors.primary"
    return ref.substr(1, ref.size() - 2);
}

// ── Section walker ──────────────────────────────────────────────────────

std::vector<std::string> walk_sections(const std::string& body) {
    std::vector<std::string> sections;
    std::istringstream iss(body);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.size() >= 3 && line[0] == '#' && line[1] == '#' && line[2] != '#') {
            std::string heading = trim(line.substr(2));
            if (!heading.empty()) sections.push_back(heading);
        }
    }
    return sections;
}

// ── Diagnostic helpers ──────────────────────────────────────────────────

DesignMdDiagnostic make_diag(DesignMdSeverity sev,
                              std::string code,
                              std::string path,
                              int line,
                              int column,
                              std::string message) {
    DesignMdDiagnostic d;
    d.severity = sev;
    d.code = std::move(code);
    d.path = std::move(path);
    d.line = line;
    d.column = column;
    d.message = std::move(message);
    return d;
}

int yaml_line(const YAML::Node& node, int fallback) {
    if (!node || !node.Mark().is_null()) {
        // yaml-cpp Marks are 0-based; convert to 1-based.
        if (node && node.Mark().line >= 0) return node.Mark().line + 1;
    }
    return fallback;
}

// ── Top-level token group walkers ───────────────────────────────────────

void walk_colors_map(const YAML::Node& node,
                      DesignMdParseResult& result) {
    if (!node || !node.IsMap()) return;
    for (auto it = node.begin(); it != node.end(); ++it) {
        std::string name = it->first.as<std::string>("");
        if (name.empty()) continue;
        // Per spec, a color token can be either a primitive (string hex) or
        // a nested map (palette with shade levels). We handle the primitive
        // case directly and walk one level for nested palettes.
        if (it->second.IsScalar()) {
            std::string value = it->second.as<std::string>("");
            if (is_token_reference(value) || looks_like_hex_color(value)) {
                result.ir.tokens.colors.emplace(name, value);
            } else {
                result.diagnostics.push_back(make_diag(
                    DesignMdSeverity::warning, "color-shape", "colors." + name,
                    yaml_line(it->second, 0), 0,
                    "expected hex color or token reference; got \"" + value + "\""));
                result.ir.tokens.colors.emplace(name, value);
            }
        } else if (it->second.IsMap()) {
            for (auto sub = it->second.begin(); sub != it->second.end(); ++sub) {
                std::string shade = sub->first.as<std::string>("");
                std::string value = sub->second.as<std::string>("");
                if (shade.empty() || value.empty()) continue;
                result.ir.tokens.colors.emplace(name + "-" + shade, value);
            }
        }
    }
}

void walk_typography_map(const YAML::Node& node,
                          DesignMdParseResult& result) {
    if (!node || !node.IsMap()) return;
    for (auto it = node.begin(); it != node.end(); ++it) {
        std::string level = it->first.as<std::string>("");
        if (level.empty() || !it->second.IsMap()) continue;
        const std::string prefix = "typography." + level + ".";
        for (auto field = it->second.begin(); field != it->second.end(); ++field) {
            std::string key = field->first.as<std::string>("");
            std::string value = field->second.as<std::string>("");
            if (key.empty()) continue;
            // fontFeature / fontVariation explicitly preserved as strings per spec.
            // fontWeight / fontSize / lineHeight / letterSpacing also flow through
            // as strings; downstream callers (Theme materializer) re-parse them.
            result.ir.tokens.strings.emplace(prefix + key, value);
        }
    }
}

void walk_dimension_map(const YAML::Node& node,
                         const std::string& prefix,
                         DesignMdParseResult& result) {
    if (!node || !node.IsMap()) return;
    for (auto it = node.begin(); it != node.end(); ++it) {
        std::string key = it->first.as<std::string>("");
        if (key.empty() || !it->second.IsScalar()) continue;
        std::string raw = it->second.as<std::string>("");
        std::string token_name = prefix + "-" + key;
        if (auto px = parse_dimension(raw)) {
            result.ir.tokens.dimensions.emplace(token_name, *px);
        } else {
            // Per spec: unknown spacing values stored as strings.
            result.ir.tokens.strings.emplace(token_name, raw);
        }
    }
}

void walk_components_map(const YAML::Node& node,
                          DesignMdParseResult& result) {
    if (!node || !node.IsMap()) return;
    for (auto it = node.begin(); it != node.end(); ++it) {
        std::string comp = it->first.as<std::string>("");
        if (comp.empty() || !it->second.IsMap()) continue;
        const std::string prefix = "components." + comp + ".";
        for (auto field = it->second.begin(); field != it->second.end(); ++field) {
            std::string key = field->first.as<std::string>("");
            std::string value = field->second.as<std::string>("");
            if (key.empty()) continue;
            result.ir.tokens.strings.emplace(prefix + key, value);
        }
    }
}

// ── Reference resolution ────────────────────────────────────────────────

std::optional<std::string> lookup_color(const IRTokens& t, const std::string& path) {
    // "colors.primary" → look up "primary" in tokens.colors
    constexpr std::string_view prefix{"colors."};
    if (path.compare(0, prefix.size(), prefix) != 0) return std::nullopt;
    auto it = t.colors.find(path.substr(prefix.size()));
    if (it == t.colors.end()) return std::nullopt;
    return it->second;
}

std::optional<std::string> lookup_dimension(const IRTokens& t, const std::string& path) {
    // "rounded.sm" → tokens.dimensions["rounded-sm"]
    auto dot = path.find('.');
    if (dot == std::string::npos) return std::nullopt;
    std::string token = path.substr(0, dot) + "-" + path.substr(dot + 1);
    auto it = t.dimensions.find(token);
    if (it == t.dimensions.end()) return std::nullopt;
    std::ostringstream oss;
    oss << it->second << "px";
    return oss.str();
}

bool looks_like_group_ref(const std::string& path) {
    // "colors" or "typography" alone, no dot, is a group reference (illegal
    // outside the components section per spec).
    return path.find('.') == std::string::npos;
}

void resolve_references(DesignMdParseResult& result) {
    auto resolve_one = [&](const std::string& value,
                            const std::string& at_path,
                            bool inside_components) -> std::string {
        if (!is_token_reference(value)) return value;
        std::string path = reference_path(value);
        if (looks_like_group_ref(path) && !inside_components) {
            result.diagnostics.push_back(make_diag(
                DesignMdSeverity::warning, "broken-ref", at_path, 0, 0,
                "non-component reference \"" + value + "\" points to a group, not a value"));
            return value;
        }
        if (auto v = lookup_color(result.ir.tokens, path))        return *v;
        if (auto v = lookup_dimension(result.ir.tokens, path))    return *v;
        // Typography (composite) refs in components — left as-is for downstream.
        if (path.compare(0, 11, "typography.") == 0 && inside_components) return value;
        result.diagnostics.push_back(make_diag(
            DesignMdSeverity::warning, "broken-ref", at_path, 0, 0,
            "could not resolve token reference \"" + value + "\""));
        return value;
    };

    for (auto& [name, value] : result.ir.tokens.colors) {
        value = resolve_one(value, "colors." + name, false);
    }
    for (auto& [name, value] : result.ir.tokens.strings) {
        const bool comp = name.compare(0, 11, "components.") == 0;
        value = resolve_one(value, name, comp);
    }
}

} // namespace

// ── Public API ──────────────────────────────────────────────────────────

DesignMdParseResult parse_designmd(const std::string& markdown) {
    DesignMdParseResult result;
    result.ir.source = DesignSource::designmd;
    result.ir.root.type = "frame";  // empty container; DESIGN.md has no screen.

    auto slice = split_frontmatter(markdown);
    result.had_frontmatter = slice.present;
    result.sections = walk_sections(slice.body);

    // Reject duplicate section headings per spec.
    {
        std::set<std::string> seen;
        for (const auto& s : result.sections) {
            auto key = lower(s);
            if (seen.count(key)) {
                result.diagnostics.push_back(make_diag(
                    DesignMdSeverity::error, "duplicate-section", s, 0, 0,
                    "duplicate section heading \"" + s + "\" — reject per spec"));
            }
            seen.insert(key);
        }
    }

    if (!slice.present) {
        result.diagnostics.push_back(make_diag(
            DesignMdSeverity::info, "no-frontmatter", "", 0, 0,
            "no YAML frontmatter found; emitting empty token set"));
        return result;
    }

    YAML::Node root;
    try {
        root = YAML::Load(slice.yaml_text);
    } catch (const YAML::ParserException& e) {
        result.diagnostics.push_back(make_diag(
            DesignMdSeverity::error, "yaml-parse",
            "<frontmatter>",
            e.mark.line + slice.yaml_start_line, e.mark.column + 1,
            e.what()));
        return result;
    } catch (const YAML::Exception& e) {
        result.diagnostics.push_back(make_diag(
            DesignMdSeverity::error, "yaml-parse", "<frontmatter>", 0, 0, e.what()));
        return result;
    }

    if (!root || !root.IsMap()) {
        result.diagnostics.push_back(make_diag(
            DesignMdSeverity::error, "yaml-shape", "<frontmatter>", 0, 0,
            "expected a YAML mapping at the top level of frontmatter"));
        return result;
    }

    // Carry name/description/version through as string tokens.
    if (root["name"])        result.ir.tokens.strings.emplace("name",        root["name"].as<std::string>(""));
    if (root["description"]) result.ir.tokens.strings.emplace("description", root["description"].as<std::string>(""));
    if (root["version"])     result.ir.tokens.strings.emplace("version",     root["version"].as<std::string>(""));

    walk_colors_map(root["colors"],       result);
    walk_typography_map(root["typography"], result);
    walk_dimension_map(root["rounded"],   "rounded", result);
    walk_dimension_map(root["spacing"],   "spacing", result);
    walk_components_map(root["components"], result);

    resolve_references(result);

    return result;
}

DesignIR parse_designmd_yaml(const std::string& markdown) {
    return parse_designmd(markdown).ir;
}

} // namespace pulp::view
