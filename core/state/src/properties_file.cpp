#include <pulp/state/properties_file.hpp>

#include <fstream>
#include <filesystem>
#include <sstream>

namespace pulp::state {

// ── PropertiesFile ──────────────────────────────────────────────────────────

bool PropertiesFile::load(std::string_view path) {
    path_ = std::string(path);
    std::ifstream in(path_);
    if (!in.is_open())
        return false;

    // Minimal JSON-like key:value parser (flat string map).
    // A full JSON parser (e.g., choc::json) should replace this in production.
    values_.clear();
    std::string line;
    while (std::getline(in, line)) {
        // Skip braces, blank lines
        if (line.empty() || line.find('{') != std::string::npos ||
            line.find('}') != std::string::npos)
            continue;
        auto colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        // Trim quotes and whitespace
        auto strip = [](std::string s) {
            while (!s.empty() && (s.front() == ' ' || s.front() == '"' || s.front() == '\t'))
                s.erase(s.begin());
            while (!s.empty() && (s.back() == ' ' || s.back() == '"' || s.back() == ',' || s.back() == '\t'))
                s.pop_back();
            return s;
        };
        std::string key = strip(line.substr(0, colon));
        std::string val = strip(line.substr(colon + 1));
        if (!key.empty())
            values_[key] = val;
    }
    return true;
}

bool PropertiesFile::save(std::string_view path) const {
    std::string p(path);
    auto parent = std::filesystem::path(p).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent);

    std::ofstream out(p);
    if (!out.is_open())
        return false;

    out << "{\n";
    bool first = true;
    for (auto& [k, v] : values_) {
        if (!first) out << ",\n";
        out << "  \"" << k << "\": \"" << v << "\"";
        first = false;
    }
    out << "\n}\n";
    return true;
}

bool PropertiesFile::save() const {
    if (path_.empty())
        return false;
    return save(path_);
}

std::optional<std::string> PropertiesFile::get_string(std::string_view key) const {
    auto it = values_.find(key);
    if (it == values_.end())
        return std::nullopt;
    return it->second;
}

void PropertiesFile::set_string(std::string_view key, std::string_view value) {
    values_[std::string(key)] = std::string(value);
}

std::optional<int64_t> PropertiesFile::get_int(std::string_view key) const {
    auto s = get_string(key);
    if (!s) return std::nullopt;
    try { return std::stoll(*s); }
    catch (...) { return std::nullopt; }
}

void PropertiesFile::set_int(std::string_view key, int64_t value) {
    set_string(key, std::to_string(value));
}

std::optional<double> PropertiesFile::get_double(std::string_view key) const {
    auto s = get_string(key);
    if (!s) return std::nullopt;
    try { return std::stod(*s); }
    catch (...) { return std::nullopt; }
}

void PropertiesFile::set_double(std::string_view key, double value) {
    set_string(key, std::to_string(value));
}

std::optional<bool> PropertiesFile::get_bool(std::string_view key) const {
    auto s = get_string(key);
    if (!s) return std::nullopt;
    return (*s == "true" || *s == "1");
}

void PropertiesFile::set_bool(std::string_view key, bool value) {
    set_string(key, value ? "true" : "false");
}

void PropertiesFile::remove(std::string_view key) {
    auto it = values_.find(key);
    if (it != values_.end())
        values_.erase(it);
}

bool PropertiesFile::contains(std::string_view key) const {
    return values_.find(key) != values_.end();
}

void PropertiesFile::clear() {
    values_.clear();
}

std::vector<std::string> PropertiesFile::keys() const {
    std::vector<std::string> result;
    result.reserve(values_.size());
    for (auto& [k, v] : values_)
        result.push_back(k);
    return result;
}

// ── ApplicationProperties ───────────────────────────────────────────────────

ApplicationProperties::ApplicationProperties(std::string_view app_name)
    : app_name_(app_name) {}

void ApplicationProperties::load() {
    auto user_dir = user_settings_dir(app_name_);
    auto common_dir = common_settings_dir(app_name_);
    user_.load(user_dir + "/" + app_name_ + ".json");
    common_.load(common_dir + "/settings.json");
}

void ApplicationProperties::save() {
    if (!user_.path().empty())
        user_.save();
    if (!common_.path().empty())
        common_.save();
}

std::string ApplicationProperties::user_settings_dir(std::string_view app_name) {
#if defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return std::string(home ? home : "/tmp") + "/Library/Preferences";
#elif defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    return std::string(appdata ? appdata : "C:\\Users\\Default\\AppData\\Roaming") +
           "\\" + std::string(app_name);
#else
    const char* home = std::getenv("HOME");
    return std::string(home ? home : "/tmp") + "/.config/" + std::string(app_name);
#endif
}

std::string ApplicationProperties::common_settings_dir(std::string_view app_name) {
#if defined(__APPLE__)
    return "/Library/Application Support/" + std::string(app_name);
#elif defined(_WIN32)
    const char* pdata = std::getenv("PROGRAMDATA");
    return std::string(pdata ? pdata : "C:\\ProgramData") + "\\" + std::string(app_name);
#else
    return "/etc/" + std::string(app_name);
#endif
}

}  // namespace pulp::state
