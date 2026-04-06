#include <pulp/tools/audio/service.hpp>
#include <pulp/tools/audio/model_store.hpp>

#include <choc/text/choc_JSON.h>

#include <array>
#include <fstream>
#include <initializer_list>
#include <sstream>

namespace fs = std::filesystem;

namespace pulp::tools::audio {

namespace {

std::string read_text_file(const fs::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) return {};

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool parse_json_file(const fs::path& path, choc::value::Value& value, std::string& error) {
    auto text = read_text_file(path);
    if (text.empty()) {
        error = "failed to read " + path.string();
        return false;
    }

    try {
        value = choc::json::parse(text);
        return true;
    } catch (const std::exception& e) {
        error = "failed to parse " + path.string() + ": " + e.what();
        return false;
    } catch (...) {
        error = "failed to parse " + path.string();
        return false;
    }
}

std::string object_string(const choc::value::ValueView& object,
                          std::initializer_list<const char*> keys) {
    if (!object.isObject()) return {};

    for (auto* key : keys) {
        if (!object.hasObjectMember(key)) continue;
        auto value = object[key];
        if (value.isVoid()) continue;
        auto text = std::string(value.toString());
        if (!text.empty()) return text;
    }

    return {};
}

double object_number(const choc::value::ValueView& object,
                     std::initializer_list<const char*> keys,
                     double fallback = 0.0) {
    if (!object.isObject()) return fallback;

    for (auto* key : keys) {
        if (!object.hasObjectMember(key)) continue;
        auto value = object[key];
        if (value.isInt32() || value.isInt64() || value.isFloat32() || value.isFloat64())
            return value.getWithDefault(fallback);
    }

    return fallback;
}

void add_string_member(choc::value::Value& object, const char* key, const std::string& value) {
    object.addMember(key, choc::value::createString(value));
}

void add_path_member(choc::value::Value& object, const char* key, const fs::path& value) {
    add_string_member(object, key, value.string());
}

} // namespace

ModelStatus query_model_status(const fs::path& pulp_home_override) {
    ModelStatus status;

    auto pulp_home = resolve_pulp_home(pulp_home_override);
    if (pulp_home.empty()) {
        status.message = "unable to resolve PULP_HOME";
        return status;
    }

    const std::array candidates = {
        pulp_home / "audio" / "model-state.json",
        pulp_home / "audio" / "model.json",
    };

    for (const auto& candidate : candidates) {
        if (!fs::exists(candidate)) continue;

        status.state_path = candidate;
        status.state_file_found = true;

        choc::value::Value value;
        std::string error;
        if (!parse_json_file(candidate, value, error)) {
            status.message = error;
            return status;
        }

        auto root = value.getView();
        status.configured_model_id = object_string(root, {
            "configured_model_id",
            "active_model_id",
            "requested_model_id",
            "model_id",
        });
        status.backend = object_string(root, {"backend"});
        status.checkpoint_ref = object_string(root, {"checkpoint_ref"});

        auto checkpoint = object_string(root, {
            "resolved_checkpoint_path",
            "checkpoint_path",
        });
        if (!checkpoint.empty()) {
            status.resolved_checkpoint_path = fs::path(checkpoint);
            status.checkpoint_exists = fs::exists(status.resolved_checkpoint_path);
        }

        if (!status.configured_model_id.empty()) {
            auto installed = read_installed_model(status.configured_model_id, pulp_home);
            if (status.backend.empty()) status.backend = installed.backend;
            if (status.checkpoint_ref.empty()) status.checkpoint_ref = installed.checkpoint_ref;
            if (status.resolved_checkpoint_path.empty() && !installed.resolved_checkpoint_path.empty()) {
                status.resolved_checkpoint_path = installed.resolved_checkpoint_path;
                status.checkpoint_exists = installed.checkpoint_exists;
            }
        }

        if (status.configured_model_id.empty()) {
            status.message = "model state file does not declare a configured model";
        } else if (status.resolved_checkpoint_path.empty()) {
            status.message = "configured model has no resolved checkpoint path";
        } else if (!status.checkpoint_exists) {
            status.message = "resolved checkpoint path does not exist";
        } else {
            status.message = "configured model is loadable";
        }

        return status;
    }

    status.state_path = candidates.front();
    status.message = "no configured audio model";
    return status;
}

BundleReadResult read_excerpt_bundle(const fs::path& bundle_path) {
    BundleReadResult result;
    result.bundle_path = bundle_path;

    if (bundle_path.empty()) {
        result.error = "bundle path is required";
        return result;
    }
    if (!fs::exists(bundle_path) || !fs::is_directory(bundle_path)) {
        result.error = "bundle path does not exist: " + bundle_path.string();
        return result;
    }

    result.manifest_path = bundle_path / "manifest.json";
    if (!fs::exists(result.manifest_path)) {
        result.error = "missing manifest.json in bundle: " + bundle_path.string();
        return result;
    }

    choc::value::Value manifest_value;
    if (!parse_json_file(result.manifest_path, manifest_value, result.error))
        return result;

    auto manifest = manifest_value.getView();
    result.tool = object_string(manifest, {"tool"});
    result.bundle_version = static_cast<int>(object_number(manifest, {"bundle_version"}, 0.0));
    result.requested_model_id = object_string(manifest, {"requested_model_id"});
    result.loaded_model_id = object_string(manifest, {"loaded_model_id"});
    result.backend = object_string(manifest, {"backend"});
    result.result_count = static_cast<std::size_t>(object_number(manifest, {"result_count"}, 0.0));

    auto model_file = object_string(manifest, {"model_file"});
    if (model_file.empty()) model_file = "model.json";
    result.model_path = bundle_path / model_file;

    if (fs::exists(result.model_path)) {
        choc::value::Value model_value;
        std::string model_error;
        if (parse_json_file(result.model_path, model_value, model_error)) {
            auto model = model_value.getView();
            if (result.requested_model_id.empty())
                result.requested_model_id = object_string(model, {"requested_model_id", "model_id"});
            if (result.loaded_model_id.empty())
                result.loaded_model_id = object_string(model, {"loaded_model_id", "model_id"});
            if (result.backend.empty())
                result.backend = object_string(model, {"backend"});
        }
    }

    auto ranked_results_file = object_string(manifest, {"ranked_results_file"});
    if (ranked_results_file.empty()) ranked_results_file = "ranked_results.json";
    result.ranked_results_path = bundle_path / ranked_results_file;

    if (!fs::exists(result.ranked_results_path)) {
        result.error = "missing ranked results file: " + result.ranked_results_path.string();
        return result;
    }

    choc::value::Value ranked_value;
    if (!parse_json_file(result.ranked_results_path, ranked_value, result.error))
        return result;

    auto ranked = ranked_value.getView();
    if (!ranked.isObject() || !ranked.hasObjectMember("results")) {
        result.error = "ranked results file does not contain a results array";
        return result;
    }

    auto results = ranked["results"];
    if (!results.isArray()) {
        result.error = "ranked results file does not contain a results array";
        return result;
    }

    for (uint32_t i = 0; i < results.size(); ++i) {
        auto item = results[i];
        if (!item.isObject()) continue;

        BundleResult parsed;
        parsed.rank = static_cast<int>(object_number(item, {"rank"}, static_cast<double>(i + 1)));
        parsed.score = object_number(item, {"score"}, 0.0);
        parsed.source_file = object_string(item, {"source_file", "source_path"});
        parsed.source_duration_ms = object_number(item, {"source_duration_ms"}, 0.0);
        parsed.start_ms = object_number(item, {"start_ms"}, 0.0);
        parsed.end_ms = object_number(item, {"end_ms"}, 0.0);
        parsed.excerpt_file = object_string(item, {"excerpt_file"});
        result.results.push_back(std::move(parsed));
    }

    if (result.result_count == 0) {
        result.result_count = result.results.size();
    }

    result.ok = true;
    return result;
}

std::string to_json(const ModelStatus& status) {
    auto object = choc::value::createObject("");
    add_path_member(object, "state_path", status.state_path);
    object.addMember("state_file_found", choc::value::createBool(status.state_file_found));
    add_string_member(object, "configured_model_id", status.configured_model_id);
    object.addMember("loadable", choc::value::createBool(status.loadable()));
    add_string_member(object, "backend", status.backend);
    add_string_member(object, "checkpoint_ref", status.checkpoint_ref);
    add_path_member(object, "resolved_checkpoint_path", status.resolved_checkpoint_path);
    object.addMember("checkpoint_exists", choc::value::createBool(status.checkpoint_exists));
    add_string_member(object, "message", status.message);
    return choc::json::toString(object, true);
}

std::string to_json(const BundleReadResult& bundle) {
    auto object = choc::value::createObject("");
    object.addMember("ok", choc::value::createBool(bundle.ok));
    add_path_member(object, "bundle_path", bundle.bundle_path);
    add_path_member(object, "manifest_path", bundle.manifest_path);
    add_path_member(object, "model_path", bundle.model_path);
    add_path_member(object, "ranked_results_path", bundle.ranked_results_path);
    object.addMember("bundle_version", choc::value::createInt64(bundle.bundle_version));
    add_string_member(object, "tool", bundle.tool);
    add_string_member(object, "requested_model_id", bundle.requested_model_id);
    add_string_member(object, "loaded_model_id", bundle.loaded_model_id);
    add_string_member(object, "backend", bundle.backend);
    object.addMember("result_count", choc::value::createInt64(static_cast<int64_t>(bundle.result_count)));

    auto results = choc::value::createEmptyArray();
    for (const auto& item : bundle.results) {
        auto value = choc::value::createObject("");
        value.addMember("rank", choc::value::createInt64(item.rank));
        value.addMember("score", choc::value::createFloat64(item.score));
        add_string_member(value, "source_file", item.source_file);
        value.addMember("source_duration_ms", choc::value::createFloat64(item.source_duration_ms));
        value.addMember("start_ms", choc::value::createFloat64(item.start_ms));
        value.addMember("end_ms", choc::value::createFloat64(item.end_ms));
        add_string_member(value, "excerpt_file", item.excerpt_file);
        results.addArrayElement(value);
    }
    object.addMember("results", results);
    add_string_member(object, "error", bundle.error);
    return choc::json::toString(object, true);
}

} // namespace pulp::tools::audio
