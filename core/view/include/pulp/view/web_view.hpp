#pragma once

// WebView embedding for Pulp
// Wraps CHOC's WebView (ISC license) to provide embedded web content
// inside plugin/app UIs — for Monaco editors, docs panels, custom UIs, etc.
//
// Usage:
//   auto wv = WebViewPanel::create(options);
//   wv->navigate("https://example.com");
//   wv->set_html("<h1>Hello</h1>");
//   wv->evaluate_js("document.title");
//   wv->bind("myFunc", [](const std::string& args) { ... });

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>

namespace pulp::view {

// Platform-native handle (NSView*, HWND, etc.)
using NativeViewHandle = void*;

struct WebViewOptions {
    bool enable_debug = false;         ///< Enable DevTools / Web Inspector
    bool transparent_background = false;
    bool accept_first_click = true;    ///< macOS: first click on unfocused view is input
    std::string custom_user_agent;     ///< Override User-Agent string
};

// Embedded web view component
// Create with WebViewPanel::create(), then attach to a parent view or
// use native_handle() to embed in a native window hierarchy.
class WebViewPanel {
public:
    using JsCallback = std::function<std::string(const std::string& args_json)>;

    static std::unique_ptr<WebViewPanel> create(const WebViewOptions& options = {});

    virtual ~WebViewPanel() = default;

    // Get the native view handle for embedding (NSView*, HWND, etc.)
    virtual NativeViewHandle native_handle() = 0;

    // Navigate to a URL
    virtual void navigate(const std::string& url) = 0;

    // Load HTML content directly
    virtual void set_html(const std::string& html) = 0;

    // Execute JavaScript in the webview context
    virtual void evaluate_js(const std::string& js) = 0;

    // Bind a C++ function callable from JavaScript as window.<name>(args)
    // The callback receives JSON args string and returns JSON result string
    virtual void bind(const std::string& name, JsCallback callback) = 0;

    // Resize the webview
    virtual void set_size(uint32_t width, uint32_t height) = 0;
};

} // namespace pulp::view
