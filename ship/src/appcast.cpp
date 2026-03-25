#include <pulp/ship/appcast.hpp>
#include <sstream>
#include <cstring>
#include <regex>
#include <algorithm>

namespace pulp::ship {

// ── Appcast XML generation ───────────────────────────────────────────────────

static std::string xml_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            default:   out += c;
        }
    }
    return out;
}

std::string Appcast::to_xml() const {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    xml << "<rss version=\"2.0\" xmlns:sparkle=\"http://www.andymatuschak.org/xml-namespaces/sparkle\">\n";
    xml << "  <channel>\n";
    xml << "    <title>" << xml_escape(title) << "</title>\n";
    xml << "    <link>" << xml_escape(link) << "</link>\n";
    xml << "    <description>" << xml_escape(description) << "</description>\n";
    xml << "    <language>en</language>\n";

    for (auto& item : items) {
        xml << "    <item>\n";
        xml << "      <title>" << xml_escape(item.title) << "</title>\n";
        if (!item.description.empty()) {
            xml << "      <description><![CDATA[" << item.description << "]]></description>\n";
        }
        xml << "      <pubDate>" << item.pub_date << "</pubDate>\n";
        xml << "      <sparkle:version>" << xml_escape(item.build_number.empty() ? item.version : item.build_number) << "</sparkle:version>\n";
        xml << "      <sparkle:shortVersionString>" << xml_escape(item.version) << "</sparkle:shortVersionString>\n";
        if (!item.minimum_os.empty()) {
            xml << "      <sparkle:minimumSystemVersion>" << xml_escape(item.minimum_os) << "</sparkle:minimumSystemVersion>\n";
        }
        xml << "      <enclosure\n";
        xml << "        url=\"" << xml_escape(item.download_url) << "\"\n";
        xml << "        length=\"" << item.file_size << "\"\n";
        xml << "        type=\"application/octet-stream\"\n";
        if (!item.ed_signature.empty()) {
            xml << "        sparkle:edSignature=\"" << item.ed_signature << "\"\n";
        }
        xml << "      />\n";
        xml << "    </item>\n";
    }

    xml << "  </channel>\n";
    xml << "</rss>\n";
    return xml.str();
}

// ── Simple appcast XML parsing ───────────────────────────────────────────────

static std::string extract_tag(const std::string& xml, const std::string& tag) {
    auto open = "<" + tag + ">";
    auto close = "</" + tag + ">";
    auto start = xml.find(open);
    if (start == std::string::npos) return {};
    start += open.size();
    auto end = xml.find(close, start);
    if (end == std::string::npos) return {};
    return xml.substr(start, end - start);
}

static std::string extract_attr(const std::string& xml, const std::string& attr) {
    auto pos = xml.find(attr + "=\"");
    if (pos == std::string::npos) return {};
    pos += attr.size() + 2;
    auto end = xml.find('"', pos);
    if (end == std::string::npos) return {};
    return xml.substr(pos, end - pos);
}

std::optional<Appcast> Appcast::from_xml(const std::string& xml) {
    if (xml.find("<rss") == std::string::npos) return std::nullopt;

    Appcast feed;
    feed.title = extract_tag(xml, "title");
    feed.link = extract_tag(xml, "link");
    feed.description = extract_tag(xml, "description");

    // Find all <item>...</item> blocks
    size_t pos = 0;
    while (true) {
        auto item_start = xml.find("<item>", pos);
        if (item_start == std::string::npos) break;
        auto item_end = xml.find("</item>", item_start);
        if (item_end == std::string::npos) break;

        auto item_xml = xml.substr(item_start, item_end - item_start + 7);

        AppcastItem item;
        item.title = extract_tag(item_xml, "title");
        item.pub_date = extract_tag(item_xml, "pubDate");
        item.version = extract_tag(item_xml, "sparkle:shortVersionString");
        item.build_number = extract_tag(item_xml, "sparkle:version");
        item.minimum_os = extract_tag(item_xml, "sparkle:minimumSystemVersion");
        item.download_url = extract_attr(item_xml, "url");
        item.ed_signature = extract_attr(item_xml, "sparkle:edSignature");

        auto length_str = extract_attr(item_xml, "length");
        if (!length_str.empty()) item.file_size = std::stoull(length_str);

        // Extract CDATA description
        auto cdata_start = item_xml.find("<![CDATA[");
        if (cdata_start != std::string::npos) {
            cdata_start += 9;
            auto cdata_end = item_xml.find("]]>", cdata_start);
            if (cdata_end != std::string::npos)
                item.description = item_xml.substr(cdata_start, cdata_end - cdata_start);
        }

        feed.items.push_back(std::move(item));
        pos = item_end + 7;
    }

    return feed;
}

// ── Version comparison ───────────────────────────────────────────────────────

int compare_versions(const std::string& a, const std::string& b) {
    auto parse = [](const std::string& v) -> std::vector<int> {
        std::vector<int> parts;
        std::istringstream ss(v);
        std::string part;
        while (std::getline(ss, part, '.')) {
            try { parts.push_back(std::stoi(part)); }
            catch (...) { parts.push_back(0); }
        }
        return parts;
    };

    auto pa = parse(a), pb = parse(b);
    size_t len = std::max(pa.size(), pb.size());
    pa.resize(len, 0);
    pb.resize(len, 0);

    for (size_t i = 0; i < len; ++i) {
        if (pa[i] < pb[i]) return -1;
        if (pa[i] > pb[i]) return 1;
    }
    return 0;
}

// ── EdDSA signing stub ───────────────────────────────────────────────────────

std::string sign_file_ed25519(const std::string& file_path,
                              const std::string& private_key_b64) {
    // Delegate to system openssl or generate_appcast tool
    // This is a stub — real Ed25519 signing requires either:
    // 1. Linking libsodium/monocypher (MIT/BSD)
    // 2. Using system openssl CLI
    // For now, return empty string indicating unsigned
    (void)file_path;
    (void)private_key_b64;
    return {};
}

} // namespace pulp::ship
