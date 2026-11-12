std::string json_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    o += buf;
                } else {
                    o += static_cast<char>(c);
                }
        }
    }
    return o;
}

// Pulls one top-level "key":"value" string field out of a small JSON blob
// -- not a real parser, just enough to read YouTube's oEmbed response
// (see GET /jukebox/oembed) without pulling in a JSON library for one
// field. Handles the few escapes oEmbed titles actually contain
// (backslash, quote, \n) and skips \uXXXX rather than decoding it, which
// is rare enough in a video title that the fallback of leaving nothing
// there is an acceptable, honest gap.
std::string oembed_json_string_field(const std::string& json, const std::string& key) {
    auto keypos = json.find("\"" + key + "\"");
    if (keypos == std::string::npos) return "";
    auto colon = json.find(':', keypos);
    if (colon == std::string::npos) return "";
    auto q1 = json.find('"', colon);
    if (q1 == std::string::npos) return "";
    std::string out;
    for (std::size_t i = q1 + 1; i < json.size(); ++i) {
        char c = json[i];
        if (c == '"') break;
        if (c == '\\' && i + 1 < json.size()) {
            char nc = json[++i];
            if (nc == 'n') out += ' ';
            else if (nc == 'u' && i + 4 < json.size()) i += 4;
            else out += nc;
        } else {
            out += c;
        }
    }
    return out;
}

// Safely wraps s as a single POSIX shell argument by replacing every
// embedded ' with '\'' (close quote, escaped literal quote, reopen
// quote) -- safe against any input regardless of what characters it
// contains. Needed anywhere user-controlled content (a submitted email
// address, in particular -- looks_like_email() only checks shape, not
// charset, so a crafted address containing a shell metacharacter is
// otherwise a real command-injection risk in a popen() call) lands in a
// shell command this file builds. The existing curl call sites that
// url_encode their input first don't need this separately -- url_encode's
// own output charset is already shell-safe -- this is for content (like
// a JSON POST body) that isn't URL-encoded.
std::string shell_single_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

