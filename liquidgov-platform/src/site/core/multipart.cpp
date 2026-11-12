#include "site/core/multipart.hpp"
#include "site/core/http.hpp"

namespace liquidgov::site {
std::string trim_copy(std::string s) {
    while (!s.empty() && (s.back()==' '||s.back()=='\t'||s.back()=='"'||s.back()=='\'')) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i]==' '||s[i]=='\t'||s[i]=='"'||s[i]=='\'')) ++i;
    return s.substr(i);
}

// The first multipart part named `field`: its header block and its data,
// as views into `body` -- no copies, since a part can be a 100 MB program.
struct MultipartPart { std::string_view headers, data; };
std::optional<MultipartPart> multipart_part(const std::string& content_type, const std::string& body_str,
                                            const char* field) {
    auto bpos = content_type.find("boundary=");
    if (bpos == std::string::npos) return std::nullopt;
    std::string boundary = trim_copy(content_type.substr(bpos + 9));
    auto semi = boundary.find(';');
    if (semi != std::string::npos) boundary.resize(semi);
    boundary = trim_copy(boundary);
    if (boundary.empty()) return std::nullopt;
    const std::string delim = "--" + boundary;
    const std::string needle = std::string("name=\"") + field + "\"";
    const std::string_view body(body_str);
    std::size_t i = 0;
    while (i < body.size()) {
        auto start = body.find(delim, i);
        if (start == std::string_view::npos) break;
        start += delim.size();
        if (start + 1 < body.size() && body[start] == '-' && body[start + 1] == '-') break;
        if (start + 1 < body.size() && body[start] == '\r' && body[start + 1] == '\n') start += 2;
        else if (start < body.size() && body[start] == '\n') start += 1;
        auto next = body.find(delim, start);
        if (next == std::string_view::npos) break;
        std::string_view part = body.substr(start, next - start);
        while (part.size() >= 2 && part[part.size() - 2] == '\r' && part.back() == '\n')
            part.remove_suffix(2);
        auto hdr_end = part.find("\r\n\r\n");
        std::size_t skip = 4;
        if (hdr_end == std::string_view::npos) { hdr_end = part.find("\n\n"); skip = 2; }
        if (hdr_end == std::string_view::npos) { i = next; continue; }
        std::string_view hdrs = part.substr(0, hdr_end);
        if (hdrs.find(needle) == std::string_view::npos) { i = next; continue; }
        return MultipartPart{hdrs, part.substr(hdr_end + skip)};
    }
    return std::nullopt;
}

std::optional<std::vector<std::uint8_t>> multipart_file(const std::string& content_type,
                                                        const std::string& body,
                                                        const char* field) {
    auto part = multipart_part(content_type, body, field);
    if (!part) return std::nullopt;
    return std::vector<std::uint8_t>(part->data.begin(), part->data.end());
}

// The filename="..." a browser sent with a file part, or "" (no name, or
// not a file part). Untrusted: sanitize_download_name before keeping it.
std::string multipart_filename(const std::string& content_type, const std::string& body, const char* field) {
    auto part = multipart_part(content_type, body, field);
    if (!part) return {};
    auto k = part->headers.find("filename=\"");
    if (k == std::string_view::npos) return {};
    k += 10;
    auto end = part->headers.find('"', k);
    if (end == std::string_view::npos) return {};
    return std::string(part->headers.substr(k, end - k));
}

// A multipart text field (not a file) is the same wire shape as a file
// part — a header block, blank line, then the value — so multipart_file
// already extracts it correctly; this just gives that a string-typed name
// at call sites that want a form field, not bytes.
std::string multipart_field(const std::string& content_type, const std::string& body,
                             const char* field) {
    auto v = multipart_file(content_type, body, field);
    if (!v) return {};
    return std::string(v->begin(), v->end());
}

// Same walk as multipart_file, but collects every part named `field`
// instead of stopping at the first -- for a checkbox group (except/friend/
// group on the /feed composer) where the same name repeats once per
// checked box.
std::vector<std::string> multipart_field_all(const std::string& content_type, const std::string& body,
                                              const char* field) {
    std::vector<std::string> out;
    auto bpos = content_type.find("boundary=");
    if (bpos == std::string::npos) return out;
    std::string boundary = trim_copy(content_type.substr(bpos + 9));
    auto semi = boundary.find(';');
    if (semi != std::string::npos) boundary.resize(semi);
    boundary = trim_copy(boundary);
    if (boundary.empty()) return out;
    const std::string delim = "--" + boundary;
    const std::string needle = std::string("name=\"") + field + "\"";
    std::size_t i = 0;
    while (i < body.size()) {
        auto start = body.find(delim, i);
        if (start == std::string::npos) break;
        start += delim.size();
        if (start + 1 < body.size() && body[start] == '-' && body[start + 1] == '-') break;
        if (start + 1 < body.size() && body[start] == '\r' && body[start + 1] == '\n') start += 2;
        else if (start < body.size() && body[start] == '\n') start += 1;
        auto next = body.find(delim, start);
        if (next == std::string::npos) break;
        std::string part = body.substr(start, next - start);
        while (part.size() >= 2 && part[part.size() - 2] == '\r' && part.back() == '\n')
            part.resize(part.size() - 2);
        auto hdr_end = part.find("\r\n\r\n");
        std::size_t skip = 4;
        if (hdr_end == std::string::npos) { hdr_end = part.find("\n\n"); skip = 2; }
        if (hdr_end != std::string::npos) {
            std::string hdrs = part.substr(0, hdr_end);
            if (hdrs.find(needle) != std::string::npos) out.push_back(part.substr(hdr_end + skip));
        }
        i = next;
    }
    return out;
}

// Same idea for a urlencoded body (parse_form itself keeps only the last
// value per key, which is right for every single-valued field on this
// site -- this is only for a checkbox group's repeated name).
std::vector<std::string> parse_form_multi(const std::string& body, const std::string& key) {
    std::vector<std::string> out;
    std::stringstream ss(body);
    std::string part;
    while (std::getline(ss, part, '&')) {
        auto eq = part.find('=');
        if (eq == std::string::npos) continue;
        if (url_decode(part.substr(0, eq)) == key) out.push_back(url_decode(part.substr(eq + 1)));
    }
    return out;
}

}  // namespace liquidgov::site
