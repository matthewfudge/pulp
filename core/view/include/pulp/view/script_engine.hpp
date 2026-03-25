#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <optional>
#include <memory>
#include <choc/javascript/choc_javascript.h>

namespace pulp::view {

// Callback for resolving module imports (returns source code or nullopt)
using ModuleResolver = std::function<std::optional<std::string>(std::string_view path)>;

// Callback for logging from JS (console.log, console.warn, etc.)
using LogCallback = std::function<void(std::string_view level, std::string_view message)>;

// JS scripting engine for plugin UIs
// Wraps choc::javascript::Context (currently QuickJS)
// Not thread-safe — all calls must happen on the same thread
class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    ScriptEngine(ScriptEngine&&) noexcept;
    ScriptEngine& operator=(ScriptEngine&&) noexcept;

    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    // Evaluate JS code synchronously, returns the result as a choc::value::Value
    // Throws choc::javascript::Error on parse/runtime errors
    choc::value::Value evaluate(const std::string& code);

    // Run JS code asynchronously (for module-style code with imports)
    void run_module(const std::string& code, ModuleResolver resolver);

    // Register a C++ function callable from JS
    using NativeFunction = choc::javascript::Context::NativeFunction;
    void register_function(const std::string& name, NativeFunction fn);

    // Invoke a global JS function by name with no arguments
    choc::value::Value invoke(std::string_view name);

    // Invoke a global JS function with arguments
    template<typename... Args>
    choc::value::Value invoke(std::string_view name, Args&&... args);

    // Set a log callback for console.log/warn/error from JS
    void set_log_callback(LogCallback callback);

    // Check if the engine is valid
    explicit operator bool() const;

private:
    choc::javascript::Context context_;
    LogCallback log_callback_;

    void setup_console();
};

// ── Template implementations ────────────────────────────────────────────────

template<typename... Args>
choc::value::Value ScriptEngine::invoke(std::string_view name, Args&&... args) {
    return context_.invoke(name, std::forward<Args>(args)...);
}

} // namespace pulp::view
