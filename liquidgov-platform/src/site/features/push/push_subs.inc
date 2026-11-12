static bool push_subs_upsert(const std::string& user, const std::string& endpoint,
                             const std::string& p256dh, const std::string& auth) {
    if (user.empty() || endpoint.size() < 12 || endpoint.compare(0, 8, "https://") != 0) return false;
    if (endpoint.find('\t') != std::string::npos || p256dh.find('\t') != std::string::npos) return false;
    if (auth.find('\t') != std::string::npos) return false;
    std::vector<std::string> keep;
    std::ifstream in("push_subs.txt");
    std::string line;
    int mine = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto t1 = line.find('\t');
        auto t2 = line.find('\t', t1 == std::string::npos ? 0 : t1 + 1);
        auto t3 = line.find('\t', t2 == std::string::npos ? 0 : t2 + 1);
        if (t1 == std::string::npos || t2 == std::string::npos || t3 == std::string::npos) continue;
        std::string ep = line.substr(t1 + 1, t2 - t1 - 1);
        if (ep == endpoint) continue;
        if (line.compare(0, t1, user) == 0) {
            if (mine >= 7) continue;
            ++mine;
        }
        keep.push_back(line);
    }
    keep.push_back(user + "\t" + endpoint + "\t" + p256dh + "\t" + auth);
    std::ofstream out("push_subs.txt.tmp", std::ios::trunc);
    if (!out) return false;
    for (auto& l : keep) out << l << "\n";
    out.close();
    return std::rename("push_subs.txt.tmp", "push_subs.txt") == 0;
}

