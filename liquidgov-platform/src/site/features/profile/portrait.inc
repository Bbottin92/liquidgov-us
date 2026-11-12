// No picture: the person's first letter in the circle, not an empty grey circle that reads as a broken image (sweep
// 2026-11-09). The first UTF-8 code point, so a name starting with a non-ASCII letter is not cut mid-character.
std::string portrait_placeholder(const std::string& username, const char* cls) {
    std::string initial;
    if (!username.empty()) {
        const unsigned char c0 = static_cast<unsigned char>(username[0]);
        std::size_t len = c0 < 0x80 ? 1 : (c0 >> 5) == 0x6 ? 2 : (c0 >> 4) == 0xE ? 3 : (c0 >> 3) == 0x1E ? 4 : 1;
        initial = username.substr(0, std::min(len, username.size()));
        if (initial.size() == 1) initial[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(initial[0])));
    }
    return std::string("<span class=\"portrait ")+cls+" ph\" aria-hidden=\"true\">"+html_escape(initial)+"</span>";
}

std::string portrait_tag(const std::string& username, const char* cls) {
    auto acc = g_store.find_by_username(username);
    if (!acc) return portrait_placeholder(username, cls);
    auto av = g_store.get(acc->id);
    if (!av || !g_blobs.meta(av->blob_id).has_value())
        return portrait_placeholder(username, cls);
    // /portrait/<username> is served with Cache-Control: max-age=300 at
    // this same URL regardless of which image is behind it, so a browser
    // that already fetched it once shows the old picture for up to five
    // minutes after a re-upload even though the server side updated
    // immediately. blob_id changes on every save (a fresh sha256 of the
    // new bytes -- see save_avatar), so using it as a cache-busting query
    // param forces a real re-fetch the moment the picture actually
    // changes, without weakening the cache header for repeat views of the
    // same picture.
    return std::string("<img class=\"portrait ")+cls+"\" src=\"/portrait/"+html_escape(username)+
           "?v="+html_escape(av->blob_id)+"\" alt=\"\" width=\"96\" height=\"96\">";
}

std::vector<std::string> people_hits(const std::string& viewer, const std::string& term) {
    std::vector<std::string> hits;
    if (term.empty()) return hits;
    if (g_store.is_findable_exact(viewer, term)) {
        if (auto found = g_store.find_by_username(term)) hits.push_back(found->username);
    }
    if (term.size() >= 3) {
        for (auto& n : g_store.typeahead(viewer, term)) {
            if (std::find(hits.begin(), hits.end(), n) == hits.end()) hits.push_back(n);
        }
    }
    return hits;
}

