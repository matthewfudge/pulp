// WebView embedding implementation
// Wraps CHOC's choc::ui::WebView for cross-platform embedded browser support.
// macOS: WKWebView, Windows: Edge WebView2, Linux: WebKitGTK

#include <pulp/view/web_view.hpp>

#include "choc/gui/choc_WebView.h"

namespace pulp::view {

class ChocWebViewPanel : public WebViewPanel {
public:
    ChocWebViewPanel(const WebViewOptions& options) {
        choc::ui::WebView::Options choc_opts;
        choc_opts.enableDebugMode = options.enable_debug;
        choc_opts.transparentBackground = options.transparent_background;
        choc_opts.acceptsFirstMouseClick = options.accept_first_click;
        choc_opts.customUserAgent = options.custom_user_agent;

        webview_ = std::make_unique<choc::ui::WebView>(choc_opts);
    }

    NativeViewHandle native_handle() override {
        if (!webview_ || !webview_->loadedOK()) return nullptr;
        return webview_->getViewHandle();
    }

    void navigate(const std::string& url) override {
        if (webview_) webview_->navigate(url);
    }

    void set_html(const std::string& html) override {
        if (webview_) webview_->setHTML(html);
    }

    void evaluate_js(const std::string& js) override {
        if (webview_) webview_->evaluateJavascript(js);
    }

    void bind(const std::string& name, JsCallback callback) override {
        if (!webview_) return;
        webview_->bind(name, [cb = std::move(callback)](const choc::value::ValueView& args) {
            // Convert choc args to JSON string for our callback
            auto json = choc::json::toString(args);
            auto result_str = cb(json);
            // Parse result back to choc value
            if (result_str.empty()) return choc::value::Value();
            try {
                return choc::json::parse(result_str);
            } catch (...) {
                return choc::value::createString(result_str);
            }
        });
    }

    void set_size(uint32_t /*width*/, uint32_t /*height*/) override {
        // CHOC WebView sizing is handled by the parent view/window.
        // The native handle is added as a child view and resized by the parent.
    }

private:
    std::unique_ptr<choc::ui::WebView> webview_;
};

std::unique_ptr<WebViewPanel> WebViewPanel::create(const WebViewOptions& options) {
    auto panel = std::make_unique<ChocWebViewPanel>(options);
    if (!panel->native_handle()) return nullptr;
    return panel;
}

} // namespace pulp::view
