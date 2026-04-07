// protocol.cpp — Inspector protocol JSON encoding/decoding

#include <pulp/inspect/protocol.hpp>

#include <choc/text/choc_JSON.h>

namespace pulp::inspect {

std::string encode_message(const InspectorMessage& msg) {
    auto obj = choc::value::createObject("");

    if (msg.id != 0)
        obj.addMember("id", choc::value::createInt64(msg.id));

    if (!msg.method.empty())
        obj.addMember("method", choc::value::createString(msg.method));

    if (msg.is_error) {
        auto error = choc::value::createObject("");
        error.addMember("message", choc::value::createString(msg.params_json));
        obj.addMember("error", error);
    } else if (msg.id != 0 && msg.method.empty()) {
        // Response — use "result" key
        if (!msg.params_json.empty() && msg.params_json != "{}") {
            try {
                obj.addMember("result", choc::json::parse(msg.params_json));
            } catch (...) {
                obj.addMember("result", choc::value::createString(msg.params_json));
            }
        }
    } else if (!msg.params_json.empty() && msg.params_json != "{}") {
        // Request or event — use "params" key
        try {
            obj.addMember("params", choc::json::parse(msg.params_json));
        } catch (...) {
            obj.addMember("params", choc::value::createString(msg.params_json));
        }
    }

    return choc::json::toString(obj, false);
}

bool decode_message(const std::string& json, InspectorMessage& out) {
    try {
        auto val = choc::json::parse(json);
        out = {};

        if (val.hasObjectMember("id"))
            out.id = val["id"].getInt64();

        if (val.hasObjectMember("method"))
            out.method = std::string(val["method"].getString());

        if (val.hasObjectMember("error")) {
            out.is_error = true;
            if (val["error"].hasObjectMember("message"))
                out.params_json = std::string(val["error"]["message"].getString());
        } else if (val.hasObjectMember("result")) {
            out.params_json = choc::json::toString(val["result"], false);
        } else if (val.hasObjectMember("params")) {
            out.params_json = choc::json::toString(val["params"], false);
        }

        return true;
    } catch (...) {
        return false;
    }
}

InspectorMessage make_request(int64_t id, std::string method, std::string params_json) {
    return {id, std::move(method), std::move(params_json), false};
}

InspectorMessage make_response(int64_t id, std::string result_json) {
    return {id, {}, std::move(result_json), false};
}

InspectorMessage make_error(int64_t id, std::string error_message) {
    return {id, {}, std::move(error_message), true};
}

InspectorMessage make_event(std::string method, std::string params_json) {
    return {0, std::move(method), std::move(params_json), false};
}

} // namespace pulp::inspect
