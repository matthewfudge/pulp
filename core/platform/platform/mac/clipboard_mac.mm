#include <pulp/platform/clipboard.hpp>

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>

namespace pulp::platform {

bool Clipboard::set_text(const std::string& text) {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        return [pb setString:[NSString stringWithUTF8String:text.c_str()]
                     forType:NSPasteboardTypeString];
    }
}

std::optional<std::string> Clipboard::get_text() {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        NSString* str = [pb stringForType:NSPasteboardTypeString];
        if (!str) return std::nullopt;
        return std::string([str UTF8String]);
    }
}

bool Clipboard::has_text() {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        return [pb stringForType:NSPasteboardTypeString] != nil;
    }
}

bool Clipboard::set_data(const std::string& type, const std::vector<uint8_t>& data) {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        NSString* ns_type = [NSString stringWithUTF8String:type.c_str()];
        NSData* ns_data = [NSData dataWithBytes:data.data() length:data.size()];
        [pb clearContents];
        return [pb setData:ns_data forType:ns_type];
    }
}

std::optional<std::vector<uint8_t>> Clipboard::get_data(const std::string& type) {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        NSString* ns_type = [NSString stringWithUTF8String:type.c_str()];
        NSData* ns_data = [pb dataForType:ns_type];
        if (!ns_data) return std::nullopt;
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
