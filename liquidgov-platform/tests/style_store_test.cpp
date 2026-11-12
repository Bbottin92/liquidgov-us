// The profile style library's store records (sty, styn, stu, str; Brandon 2026-11-12, slice 2 of profile CSS).
// A new file because committed tests stay frozen (hard rules #6847b, #1bcfd). Covers: publish/list/remove, one use
// row per (style, person) and "most used" never counting the author, "based on" = the newest use whose style still
// exists, one report per (style, reporter), dismiss, account deletion, save/load, orphans dropped on load, and rows
// with an extra trailing field still loading (so a later release can add a field without breaking rollback).
#include "liquidgov/platform/auth.hpp"
#include "liquidgov/platform/file_store.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace liquidgov::platform;

namespace {

class NeverBreached : public BreachChecker {
public:
    bool is_breached(const std::string&) const override { return false; }
};

std::string tmp_path() {
    char buf[] = "/tmp/lgp-style-XXXXXX";
    int fd = mkstemp(buf);
    assert(fd >= 0);
    close(fd);
    unlink(buf);
    return std::string(buf) + ".lgp";
}

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void append(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::app);
    out << text;
}

} // namespace

int main() {
    NeverBreached breach;
    const std::string path = tmp_path();
    std::uint64_t night = 0, sun = 0, third = 0;
    {
        FileStore store(path);
        for (const char* who : {"ann", "bob", "cat"}) {
            static int n = 0;
            assert(create_account(store, breach, who, "correct-horse-battery-staple",
                                  "+1555931000" + std::to_string(n++)).error == AuthError::None);
        }
        // publish: author, name and CSS are all required
        assert(!store.publish_style("", "x", "h1{color:red}"));
        assert(!store.publish_style("ann", "", "h1{color:red}"));
        assert(!store.publish_style("ann", "x", ""));
        assert(!store.publish_style("nobody", "x", "h1{color:red}"));
        night = store.publish_style("ann", "Night Paper", ".lg-theme{color:#7e3fa1}").value_or(0);
        sun = store.publish_style("bob", "Sun Room", ".lg-theme{color:#2b5d8a}").value_or(0);
        third = store.publish_style("ann", "Tab\tName", "h1{}").value_or(0);
        assert(night && sun && third && night < sun && sun < third);
        auto all = store.styles();
        assert(all.size() == 3 && all[0].id == third && all[2].id == night); // newest first
        assert(store.style(night)->author == "ann" && store.style(night)->name == "Night Paper");
        assert(store.style(third)->name == "Tab\tName"); // percent-encoded on disk, intact in memory
        std::cout << "publish + list ok\n";

        // use: one row per (style, person); the author's own use is never counted
        assert(!store.use_style(999, "bob"));
        assert(!store.use_style(night, "nobody"));
        assert(store.use_style(night, "bob"));
        assert(store.use_style(night, "bob"));
        assert(store.use_style(night, "ann"));
        assert(store.style_use_count(night) == 1);
        assert(store.use_style(night, "cat"));
        assert(store.style_use_count(night) == 2);
        assert(store.style_use_count(sun) == 0);
        // based on: the newest use whose style still exists
        assert(!store.style_based_on("ann") || store.style_based_on("ann")->id == night);
        assert(store.use_style(sun, "cat"));
        assert(store.style_based_on("cat")->id == sun);
        assert(store.style_based_on("bob")->id == night);
        assert(!store.style_based_on("nobody"));
        std::cout << "use count + based on ok\n";

        // report: one per (style, reporter), the later reason replaces the earlier
        assert(!store.report_style(999, "cat", "x"));
        assert(!store.report_style(sun, "nobody", "x"));
        assert(store.report_style(sun, "cat", "rude"));
        assert(store.report_style(sun, "cat", "rude words"));
        assert(store.report_style(sun, "ann", ""));
        auto reps = store.style_reports(sun);
        assert(reps.size() == 2);
        bool saw_cat = false;
        for (const auto& r : reps) if (r.reporter == "cat") { saw_cat = true; assert(r.reason == "rude words"); }
        assert(saw_cat);
        assert(store.style_reports(night).empty());
        std::cout << "reports ok\n";
    }
    {
        // everything survives a save + load
        const std::string disk = slurp(path);
        assert(disk.find("\nsty\t" + std::to_string(night) + "\tann\t") != std::string::npos);
        assert(disk.find("Tab%09Name") != std::string::npos);
        assert(disk.find("\nstyn\t") != std::string::npos);
        assert(disk.find("\nstu\t" + std::to_string(night) + "\tbob\t") != std::string::npos);
        assert(disk.find("\nstr\t" + std::to_string(sun) + "\tcat\t") != std::string::npos);
        FileStore store(path);
        assert(store.styles().size() == 3);
        assert(store.style(third)->name == "Tab\tName");
        assert(store.style_use_count(night) == 2);
        assert(store.style_based_on("cat")->id == sun);
        assert(store.style_reports(sun).size() == 2);
        // a fresh id never reuses an old one
        auto fourth = store.publish_style("cat", "Fourth", "p{}").value_or(0);
        assert(fourth > third);
        assert(store.remove_style(fourth));
        std::cout << "save + load ok\n";

        // dismiss clears reports only
        assert(store.dismiss_style_reports(sun));
        assert(store.style_reports(sun).empty());
        assert(store.style(sun));
        // remove takes the style's use and report rows with it; based-on falls back to the next newest
        assert(store.report_style(sun, "cat", "again"));
        assert(store.remove_style(sun));
        assert(!store.remove_style(sun));
        assert(!store.style(sun));
        assert(store.style_reports(sun).empty());
        assert(store.style_based_on("cat")->id == night);
        std::cout << "dismiss + remove ok\n";
    }
    {
        FileStore store(path);
        assert(!store.style(sun) && store.style(night));
        assert(slurp(path).find("\nstu\t" + std::to_string(sun) + "\t") == std::string::npos);
        // deleting an account removes its styles (with their uses), its uses and its reports
        assert(store.report_style(night, "bob", "meh"));
        auto bob = store.find_by_username("bob");
        assert(bob && store.delete_account(bob->id));
        assert(store.style_use_count(night) == 1); // cat only
        assert(store.style_reports(night).empty());
        auto ann = store.find_by_username("ann");
        assert(ann && store.delete_account(ann->id));
        assert(store.styles().empty());
        assert(!store.style_based_on("cat"));
        std::cout << "account deletion ok\n";
    }
    {
        // load: orphans are dropped, an extra trailing field is accepted (forward compatible)
        const std::string p2 = tmp_path();
        {
            FileStore store(p2);
            assert(create_account(store, breach, "dee", "correct-horse-battery-staple", "+15559320000").error
                   == AuthError::None);
            assert(store.publish_style("dee", "Base", "h1{}"));
        }
        append(p2, "sty\t50\tdee\t7\tLater\th2{}\tfuture-field\n"
                   "sty\t51\tghost\t7\tNo author\th3{}\n"
                   "stu\t50\tdee\t8\tfuture-field\n"
                   "stu\t77\tdee\t8\n"
                   "stu\t50\tghost\t8\n"
                   "str\t50\tdee\t9\twhy\tfuture-field\n"
                   "str\t77\tdee\t9\twhy\n"
                   "styn\t60\tfuture-field\n");
        FileStore store(p2);
        assert(store.style(50) && store.style(50)->name == "Later" && store.style(50)->css == "h2{}");
        assert(!store.style(51));
        assert(store.style_based_on("dee")->id == 50);
        assert(store.style_use_count(50) == 0); // dee is the author
        assert(store.style_reports(50).size() == 1);
        assert(store.style_reports(77).empty());
        assert(store.publish_style("dee", "Next", "p{}").value_or(0) >= 60);
        // a short row still fails closed
        const std::string p3 = tmp_path();
        { FileStore s3(p3); }
        append(p3, "sty\t1\tdee\n");
        bool threw = false;
        try { FileStore s3(p3); } catch (const std::exception&) { threw = true; }
        assert(threw);
        std::cout << "load rules ok\n";
    }
    std::cout << "style_store_test: all ok\n";
    return 0;
}
