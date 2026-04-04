#include <pulp/platform/clipboard.hpp>

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>

#include <map>
#include <mutex>

namespace pulp::platform {

namespace {

std::mutex g_fallback_mutex;
std::string g_fallback_text;
bool g_fallback_has_text = false;
std::map<std::string, std::vector<uint8_t>> g_fallback_data;

bool fallback_set_text(const std::string& text) {
    std::lock_guard lock(g_fallback_mutex);
    g_fallback_text = text;
    g_fallback_has_text = true;
    return true;
}

std::optional<std::string> fallback_get_text() {
    std::lock_guard lock(g_fallback_mutex);
    if (!g_fallback_has_text) {
        return std::nullopt;
    }
    return g_fallback_text;
}

bool fallback_has_text() {
    std::lock_guard lock(g_fallback_mutex);
    return g_fallback_has_text;
}

bool fallback_set_data(const std::string& type, const std::vector<uint8_t>& data) {
    std::lock_guard lock(g_fallback_mutex);
    g_fallback_data[type] = data;
    return true;
}

std::optional<std::vector<uint8_t>> fallback_get_data(const std::string& type) {
    std::lock_guard lock(g_fallback_mutex);
    const auto it = g_fallback_data.find(type);
    if (it == g_fallback_data.end()) {
        return std::nullopt;
    }
    return it->second;
}

} // namespace

bool Clipboard::set_text(const std::string& text) {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        if (!pb) {
            return fallback_set_text(text);
        }

        [pb clearContents];
        const bool ok = [pb setString:[NSString stringWithUTF8String:text.c_str()]
                              forType:NSPasteboardTypeString];
        if (!ok) {
            return fallback_set_text(text);
        }

        fallback_set_text(text);
        return true;
    }
}

std::optional<std::string> Clipboard::get_text() {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        if (!pb) {
            return fallback_get_text();
        }
        NSString* str = [pb stringForType:NSPasteboardTypeString];
        if (!str) {
            return fallback_get_text();
        }
        return std::string([str UTF8String]);
    }
}

bool Clipboard::has_text() {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        if (!pb) {
            return fallback_has_text();
        }
        return [pb stringForType:NSPasteboardTypeString] != nil || fallback_has_text();
    }
}

bool Clipboard::set_data(const std::string& type, const std::vector<uint8_t>& data) {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        if (!pb) {
            return fallback_set_data(type, data);
        }
        NSString* ns_type = [NSString stringWithUTF8String:type.c_str()];
        NSData* ns_data = [NSData dataWithBytes:data.data() length:data.size()];
        [pb clearContents];
        const bool ok = [pb setData:ns_data forType:ns_type];
        if (!ok) {
            return fallback_set_data(type, data);
        }

        fallback_set_data(type, data);
        return true;
    }
}

std::optional<std::vector<uint8_t>> Clipboard::get_data(const std::string& type) {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        if (!pb) {
            return fallback_get_data(type);
        }
        NSString* ns_type = [NSString stringWithUTF8String:type.c_str()];
        NSData* ns_data = [pb dataForType:ns_type];
        if (!ns_data) {
            return fallback_get_data(type);
        }
        auto* bytes = static_cast<const uint8_t*>([ns_data bytes]);
        return std::vector<uint8_t>(bytes, bytes + [ns_data length]);
    }
}

} // namespace pulp::platform

#else

namespace pulp::platform {
bool Clipboard::set_text(const std::string&) { return false; }
std::optional<std::string> Clipboard::get_text() { return std::nullopt; }
bool Clipboard::has_text() { return false; }
bool Clipboard::set_data(const std::string&, const std::vector<uint8_t>&) { return false; }
std::optional<std::vector<uint8_t>> Clipboard::get_data(const std::string&) { return std::nullopt; }
}

#endif
