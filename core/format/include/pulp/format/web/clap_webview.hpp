#pragma once

// CLAP Draft Webview Extension for Pulp
// Implements clap.ext.draft.webview (CLAP v1.2.7+) for WCLAP UIs.
//
// This is the primary way WCLAPs provide a GUI, since it requires no
// platform-specific view handles or headers. The host provides a webview,
// and the plugin provides HTML/JS/CSS content to render in it.
//
// The webview extension works alongside clap.gui:
//   - Plugins implement clap_plugin_webview to provide content
//   - Hosts implement clap_host_webview to provide the webview
//   - WCLAP bridge translates webview → clap.gui for native hosts
//
// Reference: https://github.com/free-audio/clap/blob/main/include/clap/ext/draft/webview.h

#include <cstdint>
#include <string>
#include <functional>

namespace pulp::format::wclap {

// Extension ID (from CLAP draft spec)
static constexpr const char* CLAP_EXT_WEBVIEW = "clap.webview/1";
static constexpr const char* CLAP_WINDOW_API_WEBVIEW = "webview";

// Plugin-side webview extension
// Provides content for the host's webview
struct WebviewContent {
    std::string url;              // URL to navigate to (for remote content)
    std::string html;             // Inline HTML (alternative to URL)
    uint32_t preferred_width = 400;
    uint32_t preferred_height = 300;
    bool resizable = true;
};

// Callback for JS→C++ communication from the webview
using WebviewMessageCallback = std::function<void(const std::string& message_json)>;

// Plugin webview interface
// Implement this on your Processor to provide webview-based UI
class WebviewProvider {
public:
    virtual ~WebviewProvider() = default;

    // Return the webview content (HTML or URL)
    virtual WebviewContent get_webview_content() const = 0;

    // Called when the webview sends a message to the plugin
    virtual void on_webview_message(const std::string& message_json) {
        (void)message_json;
    }

    // Called when the webview is shown/hidden
    virtual void on_webview_show(bool visible) { (void)visible; }

    // Send a message from the plugin to the webview
    // (Set by the host when the webview is created)
    void set_message_callback(WebviewMessageCallback cb) {
        send_to_webview_ = std::move(cb);
    }

protected:
    // Call this to send a JSON message to the webview
    void send_message(const std::string& json) {
        if (send_to_webview_) send_to_webview_(json);
    }

private:
    WebviewMessageCallback send_to_webview_;
};

// Auto-generate a simple webview UI from parameters
// Returns HTML that creates knobs/sliders for each parameter
// and communicates via postMessage for parameter changes
std::string generate_webview_html(const std::string& plugin_name,
                                   const std::string& params_json);

} // namespace pulp::format::wclap
