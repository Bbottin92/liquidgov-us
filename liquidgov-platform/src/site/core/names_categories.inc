// Group rosters and messages key on AccountId, which can outlive the
// account itself (delete_account() does not prune conversations_/
// messages_ — see file_store.cpp). Same "relabel, don't erase" choice
// already used for mail_/posts_.
std::string display_name_for(AccountId id) {
    auto acc = g_store.find_by_id(id);
    if (!acc) return "[deleted account]";
    return acc->username;
}

// List label: named groups use the title; unnamed ones list everyone else.
std::string group_label(const Conversation& conv, AccountId viewer) {
    if (!conv.title.empty()) return conv.title;
    std::string label;
    for (const auto& m : conv.members) {
        if (m.account == viewer) continue;
        if (!label.empty()) label += ", ";
        label += display_name_for(m.account);
    }
    return label.empty() ? "(just you)" : label;
}

// The fixed top-level list a community's creator picks from -- see
// CommunityCategory's own comment in file_store.hpp for why this list is
// closed while subcategory (free text) stays open. Order here is display
// order everywhere the list is rendered.
struct CategoryInfo { CommunityCategory value; const char* label; };
constexpr CategoryInfo kCommunityCategories[] = {
    {CommunityCategory::LocalCivic, "Local & Civic"},
    {CommunityCategory::PolicyGovernment, "Policy & Government"},
    {CommunityCategory::ArtsCulture, "Arts & Culture"},
    {CommunityCategory::HobbiesInterests, "Hobbies & Interests"},
    {CommunityCategory::SportsRecreation, "Sports & Recreation"},
    {CommunityCategory::Technology, "Technology"},
    {CommunityCategory::BusinessTrade, "Business & Trade"},
    {CommunityCategory::LearningEducation, "Learning & Education"},
    {CommunityCategory::SupportWellbeing, "Support & Wellbeing"},
    {CommunityCategory::FaithSpirituality, "Faith & Spirituality"},
    {CommunityCategory::General, "General"},
};
const char* category_label(CommunityCategory c) {
    for (const auto& ci : kCommunityCategories) if (ci.value == c) return ci.label;
    return "General";
}

// Every category's currently-used subcategory names, as a JS object literal
// ({0:['Neighborhood Watch',...], ...}) embedded into the creation form's
// own inline script -- lets picking a category filter the subcategory
// datalist to just that category's existing names, entirely client-side,
// no fetch needed. Same js_string_literal used for every other inline-JS
// string on this site (see jukebox_stations_js for the identical shape).
std::string community_subcats_js() {
    std::ostringstream os;
    os << "{";
    bool first_cat = true;
    for (const auto& ci : kCommunityCategories) {
        if (!first_cat) os << ",";
        first_cat = false;
        os << static_cast<int>(ci.value) << ":[";
        bool first_sub = true;
        for (const auto& s : g_store.subcategories_of(ci.value)) {
            if (!first_sub) os << ",";
            first_sub = false;
            os << js_string_literal(s);
        }
        os << "]";
    }
    os << "}";
    return os.str();
}

// "/city/<key>" for a group's city text, percent-encoded (keys can hold
// UTF-8). Empty when the text has no city key.
std::string city_href(const std::string& city) {
    const std::string key = city_key(city);
    return key.empty() ? std::string() : "/city/" + url_encode(key);
}

// <datalist id="citylist"> of every city already in use, so "Fresno, CA"
// isn't split three ways by spelling (same idea as the subcategory list).
std::string city_datalist_html() {
    std::string html = "<datalist id=\"citylist\">";
    for (const auto& cu : g_store.cities_in_use()) html += "<option value=\"" + html_escape(cu.name) + "\">";
    return html + "</datalist>";
}

