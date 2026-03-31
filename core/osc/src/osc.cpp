#include <pulp/osc/osc.hpp>
#include <cstring>
#include <algorithm>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace pulp::osc {

// ── Message accessors ────────────────────────────────────────────────────────

int32_t Message::get_int(size_t i, int32_t def) const {
    if (i >= args.size()) return def;
    if (auto* v = std::get_if<int32_t>(&args[i])) return *v;
    return def;
}

float Message::get_float(size_t i, float def) const {
    if (i >= args.size()) return def;
    if (auto* v = std::get_if<float>(&args[i])) return *v;
    return def;
}

std::string Message::get_string(size_t i, const std::string& def) const {
    if (i >= args.size()) return def;
    if (auto* v = std::get_if<std::string>(&args[i])) return *v;
    return def;
}

// ── OSC encoding helpers ─────────────────────────────────────────────────────

// Pad to 4-byte boundary
static size_t pad4(size_t n) { return (n + 3) & ~3; }

static void write_string(std::vector<uint8_t>& buf, const std::string& s) {
    buf.insert(buf.end(), s.begin(), s.end());
    buf.push_back(0); // null terminator
    // Pad to 4-byte boundary
    while (buf.size() % 4 != 0) buf.push_back(0);
}

static void write_int32(std::vector<uint8_t>& buf, int32_t v) {
    uint32_t n = htonl(static_cast<uint32_t>(v));
    auto* p = reinterpret_cast<const uint8_t*>(&n);
    buf.insert(buf.end(), p, p + 4);
}

static void write_float32(std::vector<uint8_t>& buf, float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    bits = htonl(bits);
    auto* p = reinterpret_cast<const uint8_t*>(&bits);
    buf.insert(buf.end(), p, p + 4);
}

static void write_blob(std::vector<uint8_t>& buf, const std::vector<uint8_t>& data) {
    write_int32(buf, static_cast<int32_t>(data.size()));
    buf.insert(buf.end(), data.begin(), data.end());
    while (buf.size() % 4 != 0) buf.push_back(0);
}

// ── Encode ───────────────────────────────────────────────────────────────────

std::vector<uint8_t> encode(const Message& msg) {
    std::vector<uint8_t> buf;

    // Address pattern
    write_string(buf, msg.address);

    // Type tag string
    std::string tags = ",";
    for (auto& arg : msg.args) {
        if (std::holds_alternative<int32_t>(arg)) tags += 'i';
        else if (std::holds_alternative<float>(arg)) tags += 'f';
        else if (std::holds_alternative<std::string>(arg)) tags += 's';
        else if (std::holds_alternative<std::vector<uint8_t>>(arg)) tags += 'b';
    }
    write_string(buf, tags);

    // Arguments
    for (auto& arg : msg.args) {
        if (auto* v = std::get_if<int32_t>(&arg)) write_int32(buf, *v);
        else if (auto* v = std::get_if<float>(&arg)) write_float32(buf, *v);
        else if (auto* v = std::get_if<std::string>(&arg)) write_string(buf, *v);
        else if (auto* v = std::get_if<std::vector<uint8_t>>(&arg)) write_blob(buf, *v);
    }

    return buf;
}

// ── Decode helpers ───────────────────────────────────────────────────────────

static std::string read_string(const uint8_t* data, size_t size, size_t& offset) {
    if (offset >= size) return {};
    auto* start = reinterpret_cast<const char*>(data + offset);
    auto len = strnlen(start, size - offset);
    std::string result(start, len);
    offset += pad4(len + 1);
    return result;
}

static int32_t read_int32(const uint8_t* data, size_t size, size_t& offset) {
    if (offset + 4 > size) return 0;
    uint32_t n;
    std::memcpy(&n, data + offset, 4);
    offset += 4;
    return static_cast<int32_t>(ntohl(n));
}

static float read_float32(const uint8_t* data, size_t size, size_t& offset) {
    if (offset + 4 > size) return 0;
    uint32_t bits;
    std::memcpy(&bits, data + offset, 4);
    bits = ntohl(bits);
    float v;
    std::memcpy(&v, &bits, 4);
    offset += 4;
    return v;
}

static std::vector<uint8_t> read_blob(const uint8_t* data, size_t size, size_t& offset) {
    auto blob_size = read_int32(data, size, offset);
    if (blob_size < 0 || offset + blob_size > size) return {};
    std::vector<uint8_t> result(data + offset, data + offset + blob_size);
    offset += pad4(blob_size);
    return result;
}

// ── Decode ───────────────────────────────────────────────────────────────────

Message decode(const uint8_t* data, size_t size) {
    Message msg;
    size_t offset = 0;

    msg.address = read_string(data, size, offset);
    if (msg.address.empty() || msg.address[0] != '/') return {};

    std::string tags = read_string(data, size, offset);
    if (tags.empty() || tags[0] != ',') return msg; // No args is valid

    for (size_t i = 1; i < tags.size(); ++i) {
        switch (tags[i]) {
            case 'i': msg.args.emplace_back(read_int32(data, size, offset)); break;
            case 'f': msg.args.emplace_back(read_float32(data, size, offset)); break;
            case 's': msg.args.emplace_back(read_string(data, size, offset)); break;
            case 'b': msg.args.emplace_back(read_blob(data, size, offset)); break;
            default: break;
        }
    }

    return msg;
}

} // namespace pulp::osc
