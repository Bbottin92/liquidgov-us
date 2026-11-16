#include "liquidgov/platform/file_store.hpp"

#include "core/codec.hpp"
#include "core/load_pending.hpp"

#include "liquidgov/platform/clock.hpp"

#include <sodium.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

namespace liquidgov::platform {

using namespace store_detail;

namespace {

void write_all(int fd, const std::string& data) {
    const char* p = data.data();
    std::size_t left = data.size();
    while (left > 0) {
        const ssize_t n = ::write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error(std::string("store write failed: ") + std::strerror(errno));
        }
        if (n == 0) throw std::runtime_error("store write returned zero");
        p += n;
        left -= static_cast<std::size_t>(n);
    }
}

} // namespace

FileStore::FileStore(std::string path) : path_(std::move(path)) { load(); }

void FileStore::load() {
    next_id_ = 1;
    next_post_id_ = 1;
    next_reply_id_ = 1;
    next_proposal_id_ = 1;
    next_proposal_reply_id_ = 1;
    next_note_id_ = 1;
    next_conv_id_ = 1;
    next_message_id_ = 1;
    by_username_.clear();
    accounts_.clear();
    sessions_.clear();
    blurbs_.clear();
    profiles_.clear();
    author_css_.clear();
    here_choice_.clear();
    styles_.clear();
    next_style_id_ = 1;
    style_uses_.clear();
    style_reports_.clear();
    canvas_frames_.clear();
    canvas_timelapse_.clear();
    phone_fp_.clear();
    quarantine_.clear();
    emails_.clear();
    reset_tokens_.clear();
    google_ids_.clear();
    friends_.clear();
    requests_.clear();
    mail_.clear();
    blocks_.clear();
    block_meta_.clear();
    inner_.clear();
    prefs_.clear();
    posts_.clear();
    replies_.clear();
    seconds_.clear();
    proposals_.clear();
    proposal_replies_.clear();
    proposal_reactions_.clear();
    proposal_votes_.clear();
    ballot_values_.clear();
    communities_.clear();
    members_.clear();
    notices_.clear();
    avatars_.clear();
    jukebox_.clear();
    jukebox_history_.clear();
    jukebox_subs_.clear();
    jukebox_autoplay_.clear();
    jukebox_relay_.clear();
    jukebox_pos_.clear();
    locked_.clear();
    audit_log_.clear();
    program_grants_.clear();
    attachment_names_.clear();
    conversations_.clear();
    messages_.clear();

    std::ifstream in(path_, std::ios::binary);
    if (!in) return;

    std::string line;
    if (!std::getline(in, line)) return;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "LGP1") {
        throw std::runtime_error("LGP1: bad magic in " + path_ + " (refusing to wipe)");
    }

    LoadPending pending;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto f = split_tab(line);
        if (f.empty()) throw std::runtime_error("LGP1: empty record");
        const std::string& tag = f[0];
        // Each record module reads its own types (src/records/*.cpp, read_*); no module -> unknown type.
        if (read_counters(f, pending) ||
            read_accounts(f, pending) ||
            read_social(f, pending) ||
            read_mail(f, pending) ||
            read_posts(f, pending) ||
            read_proposals(f, pending) ||
            read_proposal_comments(f, pending) ||
            read_communities(f, pending) ||
            read_city(f, pending) ||
            read_prefs(f, pending) ||
            read_pains(f, pending) ||
            read_profile(f, pending) ||
            read_notices(f, pending) ||
            read_avatars(f, pending) ||
            read_conversations(f, pending) ||
            read_jukebox(f, pending) ||
            read_admin(f, pending) ||
            read_here(f, pending) ||
            read_styles(f, pending))
            continue;
        throw std::runtime_error("LGP1: unknown record type '" + tag + "'");
    }
    prune_attachment_names(); // names whose mail was purged since the last run
    finish_here();
    finish_styles();
    finish_communities(pending);
    finish_city_lists(pending);
    finish_posts(pending);
    finish_city_tags(pending);
    finish_proposals(pending);
    finish_proposal_comments(pending);
}

void FileStore::flush() {
    namespace fs = std::filesystem;
    fs::path p(path_);
    if (p.has_parent_path() && !p.parent_path().empty()) {
        fs::create_directories(p.parent_path());
    }
    const std::string tmp = path_ + ".tmp";

    std::ostringstream body;
    body << "LGP1\n";
    // The record sections, in the order the store has always been written (each in its record module).
    write_counters(body);
    write_accounts(body);
    write_friend_links(body);
    write_mail(body);
    write_posts(body);
    write_proposals(body);
    write_proposal_comments(body);
    write_communities(body);
    write_city_lists(body);
    write_map_prefs(body);
    write_hints(body);
    write_pains(body);
    write_city_tags(body);
    write_members(body);
    write_blocks(body);
    write_prefs(body);
    write_profiles(body);
    write_notices(body);
    write_avatars(body);
    write_author_css(body);
    write_here(body);
    write_styles(body);
    write_canvas(body);
    write_jukebox(body);
    write_admin(body);
    write_attachment_names(body);
    write_conversations(body);
    write_mail_read(body);
    const std::string data = body.str();

    const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        throw std::runtime_error(std::string("store open failed: ") + std::strerror(errno));
    }
    try {
        write_all(fd, data);
        if (::fsync(fd) != 0) {
            const int e = errno;
            throw std::runtime_error(std::string("store fsync failed: ") + std::strerror(e));
        }
    } catch (...) {
        ::close(fd);
        ::unlink(tmp.c_str());
        throw;
    }
    if (::close(fd) != 0) {
        ::unlink(tmp.c_str());
        throw std::runtime_error(std::string("store close failed: ") + std::strerror(errno));
    }
    if (::rename(tmp.c_str(), path_.c_str()) != 0) {
        const int e = errno;
        ::unlink(tmp.c_str());
        throw std::runtime_error(std::string("store rename failed: ") + std::strerror(e));
    }
}

} // namespace liquidgov::platform
