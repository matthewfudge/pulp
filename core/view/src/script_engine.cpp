// Include the QuickJS implementation — must appear in exactly one translation unit
#include <choc/javascript/choc_javascript_QuickJS.h>
#include <choc/javascript/choc_javascript_Console.h>

#include <pulp/view/script_engine.hpp>
#include <sstream>

namespace pulp::view {

// ── QuickJS stack size increase ─────────────────────────────────────────────
// The web-compat prelude + deep JS↔C++ interleaving during DOM operations
// (appendChild → _reparentNative → native bridge → back to JS) exhausts
// QuickJS's default 256KB JS stack. We increase it to 1MB.
//
// CHOC's Context::pimpl is private, but we need the JSRuntime* to call
// JS_SetMaxStackSize. Since script_engine.cpp includes the full QuickJS
// implementation, we have access to quickjs::QuickJSContext (which has a
// public JSRuntime* runtime member). We access it through the Context's
// known single-member layout: { unique_ptr<Pimpl> pimpl }.

static void set_quickjs_stack_size(choc::javascript::Context& ctx, size_t size) {
    // Context has exactly one member: unique_ptr<Pimpl> pimpl
    // Mirror that layout to access the pointer
    struct ContextLayout {
        std::unique_ptr<choc::javascript::Context::Pimpl> pimpl;
    };
    auto& layout = reinterpret_cast<ContextLayout&>(ctx);
    auto* qjctx = static_cast<choc::javascript::quickjs::QuickJSContext*>(layout.pimpl.get());
    if (qjctx && qjctx->runtime)
        JS_SetMaxStackSize(qjctx->runtime, size);
}

ScriptEngine::ScriptEngine()
    : context_(choc::javascript::createQuickJSContext())
{
    set_quickjs_stack_size(context_, 1024 * 1024);  // 1MB (up from 256KB)
    setup_console();
}

ScriptEngine::~ScriptEngine() = default;

ScriptEngine::ScriptEngine(ScriptEngine&&) noexcept = default;
ScriptEngine& ScriptEngine::operator=(ScriptEngine&&) noexcept = default;

choc::value::Value ScriptEngine::evaluate(const std::string& code) {
    return context_.evaluateExpression(code);
}

void ScriptEngine::run_module(const std::string& code, ModuleResolver resolver) {
    context_.runModule(code,
        [resolver = std::move(resolver)](std::string_view path) -> std::optional<std::string> {
            return resolver(path);
        });
}

void ScriptEngine::register_function(const std::string& name, NativeFunction fn) {
    context_.registerFunction(name, std::move(fn));
}

choc::value::Value ScriptEngine::invoke(std::string_view name) {
    return context_.invoke(name);
}

void ScriptEngine::set_log_callback(LogCallback callback) {
    log_callback_ = std::move(callback);
    setup_console();
}

ScriptEngine::operator bool() const {
    return static_cast<bool>(context_);
}

static std::string_view logging_level_name(choc::javascript::LoggingLevel level) {
    switch (level) {
        case choc::javascript::LoggingLevel::log:   return "log";
        case choc::javascript::LoggingLevel::info:  return "info";
        case choc::javascript::LoggingLevel::warn:  return "warn";
        case choc::javascript::LoggingLevel::error: return "error";
        case choc::javascript::LoggingLevel::debug: return "debug";
        default: return "log";
    }
}

void ScriptEngine::setup_console() {
    choc::javascript::registerConsoleFunctions(context_,
        [this](std::string_view message, choc::javascript::LoggingLevel level) {
            if (log_callback_) {
                log_callback_(logging_level_name(level), message);
            }
        });
}

} // namespace pulp::view
