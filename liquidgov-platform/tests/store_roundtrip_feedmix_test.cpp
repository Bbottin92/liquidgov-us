// The LGP1 store reads and writes the same bytes for the record type of the feed's mix-in setting (fmix, feed filters
// slice 5, Brandon 2026-11-20), on top of every older type. A new file: the earlier round-trip tests are frozen. Same
// method as tests/store_roundtrip_hints_test.cpp (see tests/store_roundtrip_styles_here_test.cpp for the case list).
//
// Input: tests/fixtures/lgp1_roundtrip_feedmix/store_all_tags.lgp1 = the hints fixture plus three fmix rows (off with no
// tab list; 1 in 3 with two tab choices; 1 in 10, an empty tab list and an extra field kept verbatim).
// Expected answers were written by this code with --write (wip/165-feed-mix):
//   store_all_tags.expected.lgp1  the saved store for the as-is case (byte for byte)
//   cases.expected.txt           one line per case
// The saved expected store, loaded and saved again, must not change (a second round trip is stable).
#include "liquidgov/platform/file_store.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using namespace liquidgov::platform;

namespace {

const std::string kDir = std::string(LG_SOURCE_DIR) + "/tests/fixtures/lgp1_roundtrip_feedmix/";
const std::string kTmp = "store_roundtrip_feedmix_test.tmp.lgp";

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::string();
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void spit(const std::string& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << data;
}

std::string fnv(const std::string& s) {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(h));
    return buf;
}

// Load the store text, save it once, return the saved bytes (or "ERR <message>").
std::string round_trip(const std::string& text, bool& ok) {
    spit(kTmp, text);
    ok = false;
    try {
        FileStore store(kTmp);
        const auto accounts = store.all_accounts();
        if (accounts.empty()) return "ERR (no account to save through)";
        store.set_blurb(accounts.front().id, store.blurb(accounts.front().id));
    } catch (const std::exception& e) {
        std::string msg = e.what();
        for (auto at = msg.find(kTmp); at != std::string::npos; at = msg.find(kTmp)) msg.replace(at, kTmp.size(), "<path>");
        return "ERR " + msg;
    }
    ok = true;
    return slurp(kTmp);
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    for (std::string l; std::getline(in, l);) out.push_back(l);
    return out;
}

std::string join(const std::vector<std::string>& lines) {
    std::string s;
    for (const auto& l : lines) s += l + "\n";
    return s;
}

} // namespace

int main(int argc, char** argv) {
    const bool write = argc > 1 && std::string(argv[1]) == "--write";
    const std::string fixture = slurp(kDir + "store_all_tags.lgp1");
    assert(!fixture.empty());
    const auto rows = lines_of(fixture);
    assert(rows.size() > 75 && rows[0] == "LGP1");

    bool ok = false;
    const std::string saved = round_trip(fixture, ok);
    assert(ok);

    std::string cases;
    auto add_case = [&](const std::string& name, const std::string& text) {
        bool case_ok = false;
        const std::string r = round_trip(text, case_ok);
        cases += name + "\t" + (case_ok ? "OK " + fnv(r) : r) + "\n";
    };
    add_case("as-is", fixture);
    for (std::size_t i = 1; i < rows.size(); ++i) {
        auto shorter = rows;
        const auto tab = shorter[i].rfind('\t');
        shorter[i] = tab == std::string::npos ? std::string("") : shorter[i].substr(0, tab);
        add_case("drop-last-field:" + std::to_string(i) + ":" + rows[i].substr(0, rows[i].find('\t')), join(shorter));
        auto longer = rows;
        longer[i] += "\tx";
        add_case("extra-field:" + std::to_string(i) + ":" + rows[i].substr(0, rows[i].find('\t')), join(longer));
    }
    {
        auto bad = rows;
        bad[0] = "LGP2";
        add_case("bad-magic", join(bad));
        auto unknown = rows;
        unknown.push_back("zzz\t1");
        add_case("unknown-type", join(unknown));
    }

    if (write) {
        spit(kDir + "store_all_tags.expected.lgp1", saved);
        spit(kDir + "cases.expected.txt", cases);
        std::remove(kTmp.c_str());
        std::cout << "wrote expected answers\n";
        return 0;
    }

    const std::string want_saved = slurp(kDir + "store_all_tags.expected.lgp1");
    const std::string want_cases = slurp(kDir + "cases.expected.txt");
    int fails = 0;
    if (saved != want_saved) {
        ++fails;
        const auto a = lines_of(saved), b = lines_of(want_saved);
        for (std::size_t i = 0; i < std::max(a.size(), b.size()); ++i) {
            if (i >= a.size() || i >= b.size() || a[i] != b[i]) {
                std::cout << "FAIL saved store differs first at line " << i + 1 << "\n  got:  "
                          << (i < a.size() ? a[i] : "<end>") << "\n  want: " << (i < b.size() ? b[i] : "<end>") << "\n";
                break;
            }
        }
    } else {
        std::cout << "PASS saved store is byte-identical (" << saved.size() << " bytes)\n";
    }
    if (cases != want_cases) {
        ++fails;
        const auto a = lines_of(cases), b = lines_of(want_cases);
        int shown = 0;
        for (std::size_t i = 0; i < std::max(a.size(), b.size()) && shown < 10; ++i) {
            if (i >= a.size() || i >= b.size() || a[i] != b[i]) {
                ++shown;
                std::cout << "FAIL case differs\n  got:  " << (i < a.size() ? a[i] : "<end>")
                          << "\n  want: " << (i < b.size() ? b[i] : "<end>") << "\n";
            }
        }
    } else {
        std::cout << "PASS " << lines_of(cases).size() << " cases (bad rows, extra fields, errors) answer as before\n";
    }
    bool again_ok = false;
    const std::string again = round_trip(want_saved, again_ok);
    if (!again_ok || again != want_saved) {
        ++fails;
        std::cout << "FAIL the expected store changes when loaded and saved again\n";
    } else {
        std::cout << "PASS a second round trip is stable\n";
    }
    std::remove(kTmp.c_str());
    std::remove((kTmp + ".tmp").c_str());
    std::cout << "passes=" << (3 - fails) << " fails=" << fails << "\n";
    return fails ? 1 : 0;
}
