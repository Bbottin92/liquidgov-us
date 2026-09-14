#include "liquidgov/platform/auth.hpp"
#include "liquidgov/platform/file_store.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

using namespace liquidgov::platform;

namespace {

class NeverBreached : public BreachChecker {
public:
    bool is_breached(const std::string&) const override { return false; }
};

// Minimal stand-in for the site's real SocialGraph adapter (http_server.cpp):
// friends through FileStore's username graph, is_minor always false since
// there is no AgeAssurance yet — same rule the site uses.
class TestSocialGraph : public SocialGraph {
public:
    explicit TestSocialGraph(const FileStore& store) : store_(store) {}
    bool are_friends(AccountId a, AccountId b) const override {
        auto ua = store_.find_by_id(a);
        auto ub = store_.find_by_id(b);
        if (!ua || !ub) return false;
        return store_.are_friends(ua->username, ub->username);
    }
    bool is_minor(AccountId) const override { return false; }

private:
    const FileStore& store_;
};

class AlwaysAllow : public RateLimiter {
public:
    bool allow(const std::string&, const std::string&) override { return true; }
    void record_attempt(const std::string&, const std::string&) override {}
};

std::string tmp_path() {
    char buf[] = "/tmp/lgp-store-XXXXXX";
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

} // namespace

int main() {
    NeverBreached breach;
    AlwaysAllow limiter;
    const std::string path = tmp_path();

    assert(valid_username("brandon"));
    assert(valid_username("A.B_c-9"));
    assert(!valid_username(""));
    assert(!valid_username("has space"));
    assert(!valid_username("has/slash"));
    std::cout << "username charset ok\n";

    AccountId id = 0;
    SessionId sess_id;
    {
        FileStore store(path);
        auto signup = create_account(store, breach, "brandon", "correct-horse-battery-staple", "+15551212");
        assert(signup.error == AuthError::None);
        auto acc = store.find_by_username("brandon");
        assert(acc.has_value());
        id = acc->id;
        store.set_blurb(id, "hello\twith\nweird%chars");
        auto login_r = login(store, limiter, "brandon", "correct-horse-battery-staple", "test-device", "127.0.0.1");
        assert(login_r.error == AuthError::None);
        assert(login_r.session.has_value());
        sess_id = login_r.session->id;
        store.add_friend_request("brandon", "nobody-yet");
        store.add_mail("brandon", "friend", "secret body");
        auto post_id = store.add_post("brandon", "hello feed", "");
        assert(post_id >= 1);
        Community c;
        c.slug = "hall";
        c.name = "Town hall";
        c.owner = "brandon";
        c.about = "talk";
        c.created_at = 1;
        assert(store.create_community(c));
        assert(store.is_member("hall", "brandon"));
        store.block("brandon", "stranger");
        assert(store.is_blocked("brandon", "stranger"));
        assert(!store.is_findable_exact("brandon", "stranger"));
        std::cout << "created + flushed\n";
    }

    const std::string on_disk = slurp(path);
    assert(on_disk.compare(0, 5, "LGP1\n") == 0);
    assert(on_disk.find("correct-horse-battery-staple") == std::string::npos);
    assert(on_disk.find("a\t") != std::string::npos);
    assert(on_disk.find("s\t") != std::string::npos);
    std::cout << "file is LGP1 and does not contain the password\n";

    {
        FileStore store(path);
        auto acc = store.find_by_username("brandon");
        assert(acc.has_value());
        assert(acc->id == id);
        assert(acc->phone_verified);
        assert(acc->password_hash.find("$argon2") != std::string::npos);
        assert(store.blurb(id) == "hello\twith\nweird%chars");
        auto sess = store.get_session(sess_id);
        assert(sess.has_value());
        assert(sess->account == id);
        assert(sess->device_label == "test-device");
        auto again = login(store, limiter, "brandon", "correct-horse-battery-staple", "device-2", "127.0.0.1");
        assert(again.error == AuthError::None);
        assert(store.mail_with("brandon", "friend").size() == 1);
        assert(store.mail_with("brandon", "friend")[0].text == "secret body");
        assert(store.feed_for("brandon", 10).size() == 1);
        assert(store.community("hall").has_value());
        assert(store.is_blocked("brandon", "stranger"));
        std::cout << "reload: login, blurb, session, mail, feed, community survive\n";
    }

    {
        const std::string bad = tmp_path();
        {
            std::ofstream out(bad);
            out << "NOTLGP1\n";
        }
        bool threw = false;
        try {
            FileStore store(bad);
        } catch (const std::runtime_error& e) {
            threw = true;
            std::string msg = e.what();
            assert(msg.find("bad magic") != std::string::npos);
        }
        assert(threw);
        unlink(bad.c_str());
        std::cout << "bad magic is fail-closed\n";
    }

    {
        FileStore store(path);
        auto second = login(store, limiter, "brandon", "correct-horse-battery-staple", "keep", "127.0.0.1");
        assert(second.session.has_value());
        logout_all_devices(store, id, second.session->id);
        int live = 0;
        for (const auto& s : store.sessions_for(id)) {
            if (!s.revoked) ++live;
        }
        assert(live == 1);
        assert(store.get_session(second.session->id).has_value());
        std::cout << "logout_all_devices keeps current session\n";
    }

    {
        FileStore store(path);
        auto other = create_account(store, breach, "other", "correct-horse-battery-staple", "+15550000");
        assert(other.error == AuthError::None);
        store.add_friend_request("brandon", "other");
        assert(store.accept_friend_request("brandon", "other"));
        for (int i = 0; i < 12; ++i) {
            std::string n = "p" + std::to_string(i);
            create_account(store, breach, n, "correct-horse-battery-staple", "+1555"+std::to_string(10000+i));
            store.add_friend_request("brandon", n);
            store.accept_friend_request("brandon", n);
            assert(store.add_inner("brandon", n));
        }
        assert(!store.add_inner("brandon", "other"));
        assert(store.inner_of("brandon").size() == 12);

        AccountPrefs pr;
        pr.group_invite_confirm = true;
        store.set_prefs("other", pr);
        assert(!store.invite_community("hall", "brandon", "other"));

        auto acc = store.find_by_username("other");
        auto fp = fingerprint_phone("+15550000");
        store.set_phone_fp(acc->id, fp);
        assert(store.delete_account(acc->id));
        assert(!store.find_by_username("other"));
        assert(store.phone_quarantined(fp));
        std::cout << "inner cap, invite refuse, delete+quarantine ok\n";
    }

    {
        FileStore store(path);
        auto acc = store.find_by_username("brandon");
        assert(acc.has_value());
        Avatar av;
        av.owner = acc->id;
        av.blob_id = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        av.created_at = 42;
        store.set(av);
        assert(store.get(acc->id).has_value());
        assert(store.get(acc->id)->blob_id == av.blob_id);
        assert(store.all_avatars().size() == 1);
    }
    {
        FileStore store(path);
        auto acc = store.find_by_username("brandon");
        auto av = store.get(acc->id);
        assert(av.has_value());
        assert(av->blob_id.find("0123456789abcdef") == 0);
        assert(av->created_at == 42);
        const std::string on_disk = slurp(path);
        assert(on_disk.find("\nav\t") != std::string::npos);
        store.erase(acc->id);
        assert(!store.get(acc->id).has_value());
        std::cout << "avatar LGP1 tag survives reload\n";
    }

    {
        assert(extract_youtube_id("https://www.youtube.com/watch?v=dQw4w9WgXcQ") == "dQw4w9WgXcQ");
        assert(extract_youtube_id("https://www.youtube.com/watch?list=x&v=dQw4w9WgXcQ&t=30s") == "dQw4w9WgXcQ");
        assert(extract_youtube_id("https://youtu.be/dQw4w9WgXcQ") == "dQw4w9WgXcQ");
        assert(extract_youtube_id("https://youtu.be/dQw4w9WgXcQ?t=5") == "dQw4w9WgXcQ");
        assert(extract_youtube_id("https://www.youtube.com/embed/dQw4w9WgXcQ?autoplay=1") == "dQw4w9WgXcQ");
        assert(extract_youtube_id("https://www.youtube-nocookie.com/embed/dQw4w9WgXcQ") == "dQw4w9WgXcQ");
        // Not https, not an allow-listed host, an id that's the wrong
        // length, or a host that merely contains "youtube.com" as a
        // substring/suffix — every one of these must fail closed.
        assert(!extract_youtube_id("http://www.youtube.com/watch?v=dQw4w9WgXcQ"));
        assert(!extract_youtube_id("https://evil.example/watch?v=dQw4w9WgXcQ"));
        assert(!extract_youtube_id("https://youtube.com.evil.example/watch?v=dQw4w9WgXcQ"));
        assert(!extract_youtube_id("https://evil.example/?u=https://www.youtube.com/watch?v=dQw4w9WgXcQ"));
        assert(!extract_youtube_id("https://www.youtube.com/watch?v=short"));
        assert(!extract_youtube_id("not a url at all"));
        assert(!extract_youtube_id(""));
        std::cout << "extract_youtube_id: allow-list holds, no substring/scheme bypass\n";
    }

    {
        FileStore store(path);
        auto signup = create_account(store, breach, "juke", "correct-horse-battery-staple", "+15559999");
        assert(signup.error == AuthError::None);

        assert(!store.add_jukebox_track("nobody-such-account", "dQw4w9WgXcQ", "juke"));
        assert(!store.add_jukebox_track("juke", "too-short", "juke")); // defensive: not 11 chars

        assert(store.add_jukebox_track("juke", "dQw4w9WgXcQ", "juke"));
        assert(store.add_jukebox_track("juke", "aaaaaaaaaaa", "juke"));
        assert(store.jukebox_of("juke").size() == 2);
        assert(store.jukebox_of("juke")[0].video_id == "dQw4w9WgXcQ");

        store.remove_jukebox_track("juke", "dQw4w9WgXcQ");
        assert(store.jukebox_of("juke").size() == 1);
        assert(store.jukebox_of("juke")[0].video_id == "aaaaaaaaaaa");
        store.remove_jukebox_track("juke", "not-queued-x"); // no-op, doesn't throw

        for (std::size_t i = store.jukebox_of("juke").size(); i < kJukeboxQueueCap; ++i) {
            std::string id(11, 'b');
            id[0] = static_cast<char>('a' + (i % 26));
            assert(store.add_jukebox_track("juke", id, "juke"));
        }
        assert(store.jukebox_of("juke").size() == kJukeboxQueueCap);
        assert(!store.add_jukebox_track("juke", "zzzzzzzzzzz", "juke")); // full
        std::cout << "jukebox add/remove/cap ok\n";
    }

    {
        FileStore store(path);
        auto signup = create_account(store, breach, "jukeguest", "correct-horse-battery-staple", "+15558123");
        assert(signup.error == AuthError::None);

        assert(store.add_jukebox_track("jukeguest", "cccccccccc1", "jukeguest")); // owner's own
        assert(store.add_jukebox_track("jukeguest", "ddddddddddd", "someguest")); // a guest request
        assert(store.jukebox_of("jukeguest").size() == 2);
        assert(store.jukebox_of("jukeguest")[1].queued_by == "someguest");

        // A guest track is removed on completion and logged to history; an
        // owner-curated one stays queued (unaffected) but still logs.
        store.complete_jukebox_track("jukeguest", "cccccccccc1");
        assert(store.jukebox_of("jukeguest").size() == 2); // owner's own track survives
        store.complete_jukebox_track("jukeguest", "ddddddddddd");
        assert(store.jukebox_of("jukeguest").size() == 1); // the guest's is gone
        assert(store.jukebox_of("jukeguest")[0].video_id == "cccccccccc1");
        store.complete_jukebox_track("jukeguest", "not-queued-x"); // no-op, doesn't throw

        auto hist = store.jukebox_history_of("jukeguest");
        assert(hist.size() == 2);
        assert(hist[0].video_id == "cccccccccc1" && hist[0].queued_by == "jukeguest");
        assert(hist[1].video_id == "ddddddddddd" && hist[1].queued_by == "someguest");

        assert(store.add_jukebox_track("jukeguest", "eeeeeeeeeee", "jukeguest"));
        store.skip_jukebox_track("jukeguest", "cccccccccc1");
        auto skipped = store.jukebox_of("jukeguest");
        assert(skipped.size() == 2);
        assert(skipped[0].video_id == "eeeeeeeeeee");
        assert(skipped[1].video_id == "cccccccccc1");
        std::cout << "jukebox queued_by, complete, skip, and history ok\n";
    }

    {
        const std::string on_disk = slurp(path);
        assert(on_disk.find("\njt\t") != std::string::npos);
        assert(on_disk.find("\njh\t") != std::string::npos);
        FileStore store(path);
        assert(store.jukebox_of("juke").size() == kJukeboxQueueCap);
        assert(store.jukebox_of("jukeguest").size() == 2);
        assert(store.jukebox_of("jukeguest")[0].queued_by == "jukeguest");
        assert(store.jukebox_history_of("jukeguest").size() == 3);
        auto acc = store.find_by_username("juke");
        assert(acc.has_value());
        assert(store.delete_account(acc->id));
        assert(store.jukebox_of("juke").empty());
        std::cout << "jukebox LGP1 tag survives reload; purged on account delete\n";
    }

    {
        FileStore store(path);
        auto signup = create_account(store, breach, "prefuser", "correct-horse-battery-staple", "+15557001");
        assert(signup.error == AuthError::None);
        auto pr = store.prefs_of("prefuser");
        assert(pr.jukebox_policy == JukeboxPolicy::OwnerOnly); // default before any save
        assert(pr.profile_visibility == Visibility::Public && pr.posts_visibility == Visibility::Public);
        pr.jukebox_policy = JukeboxPolicy::Friends;
        pr.profile_visibility = Visibility::Friends;
        pr.posts_visibility = Visibility::OnlyMe;
        store.set_prefs("prefuser", pr);
    }
    {
        FileStore store(path); // reload
        auto pr = store.prefs_of("prefuser");
        assert(pr.jukebox_policy == JukeboxPolicy::Friends);
        assert(pr.profile_visibility == Visibility::Friends);
        assert(pr.posts_visibility == Visibility::OnlyMe);
        std::cout << "jukebox policy and visibility prefs survive reload\n";
    }

    {
        FileStore store(path);
        auto signup = create_account(store, breach, "blockee", "correct-horse-battery-staple", "+15557002");
        assert(signup.error == AuthError::None);
        store.block("prefuser", "blockee", "test reason");
        auto entries = store.block_entries_of("prefuser");
        assert(entries.size() == 1);
        assert(entries[0].other == "blockee" && entries[0].reason == "test reason" && entries[0].at > 0);
    }
    {
        FileStore store(path); // reload
        auto entries = store.block_entries_of("prefuser");
        assert(entries.size() == 1);
        assert(entries[0].other == "blockee" && entries[0].reason == "test reason" && entries[0].at > 0);
        assert(store.is_blocked("prefuser", "blockee"));
        std::cout << "block reason and timestamp survive reload\n";
    }

    std::uint64_t conv_id = 0;
    std::uint64_t msg_id = 0;
    {
        FileStore store(path);
        auto a = create_account(store, breach, "gc-alice", "correct-horse-battery-staple", "+15551110000");
        auto b = create_account(store, breach, "gc-bob", "correct-horse-battery-staple", "+15551110001");
        assert(a.error == AuthError::None && b.error == AuthError::None);
        store.add_friend_request("gc-alice", "gc-bob");
        store.accept_friend_request("gc-alice", "gc-bob");
        assert(store.are_friends("gc-alice", "gc-bob"));

        auto alice = store.find_by_username("gc-alice");
        auto bob = store.find_by_username("gc-bob");
        TestSocialGraph graph(store);
        auto created = create_conversation(store, graph, alice->id, {bob->id}, /*is_group=*/true);
        assert(created.error == MessengerError::None);
        conv_id = *created.conversation_id;

        auto sent = send_message(store, alice->id, conv_id, "hello group");
        assert(sent.error == MessengerError::None);
        msg_id = *sent.message_id;
        auto edited = edit_message(store, alice->id, msg_id, "hello group, edited");
        assert(edited.error == MessengerError::None);
        std::cout << "messenger conversation/message round-trip through FileStore ok\n";
    }
    {
        const std::string on_disk = slurp(path);
        assert(on_disk.find("\ngc\t") != std::string::npos);
        assert(on_disk.find("\ngcm\t") != std::string::npos);
        assert(on_disk.find("\ngmsg\t") != std::string::npos);
        assert(on_disk.find("\ngmh\t") != std::string::npos);
        FileStore store(path);
        auto conv = store.get_conversation(conv_id);
        assert(conv.has_value());
        assert(conv->members.size() == 2);
        auto msg = store.get_message(msg_id);
        assert(msg.has_value());
        assert(msg->text == "hello group, edited");
        assert(msg->edit_history.size() == 1);
        assert(msg->edit_history[0] == "hello group");
        std::cout << "gc/gcm/gmsg/gmh LGP1 tags survive reload\n";
    }
    {
        // Unfriending routes through on_relationship_ended: the initiator
        // (gc-alice, who unfriends here) leaves the shared group; the other
        // party (gc-bob) stays. Age-blind, same function block() uses too.
        FileStore store(path);
        auto alice = store.find_by_username("gc-alice");
        auto bob = store.find_by_username("gc-bob");
        store.unfriend("gc-alice", "gc-bob");
        auto conv = store.get_conversation(conv_id);
        assert(conv.has_value());
        bool alice_present = false, bob_present = false;
        for (auto& m : conv->members) {
            if (m.account == alice->id) alice_present = true;
            if (m.account == bob->id) bob_present = true;
        }
        assert(!alice_present);
        assert(bob_present);
        std::cout << "unfriend() calls on_relationship_ended: initiator leaves the shared group\n";
    }

    {
        FileStore store(path);
        auto a = create_account(store, breach, "prop-a", "correct-horse-battery-staple", "+15559100000");
        auto b = create_account(store, breach, "prop-b", "correct-horse-battery-staple", "+15559100001");
        assert(a.error == AuthError::None && b.error == AuthError::None);

        auto id = store.add_proposal("prop-a", "Adopt a flag", "Something simple, not a seal.");
        assert(store.proposal_by_id(id)->title == "Adopt a flag");
        assert(!store.proposal_vote_of(id, "prop-b").has_value());

        store.cast_proposal_vote(id, "prop-a", ProposalVote::Yes);
        store.cast_proposal_vote(id, "prop-b", ProposalVote::No);
        auto t = store.tally_of(id);
        assert(t.yes == 1 && t.no == 1 && t.abstain == 0);

        // Re-voting overwrites, doesn't add a second ballot.
        store.cast_proposal_vote(id, "prop-b", ProposalVote::Abstain);
        t = store.tally_of(id);
        assert(t.yes == 1 && t.no == 0 && t.abstain == 1);
        assert(store.proposal_vote_of(id, "prop-b") == ProposalVote::Abstain);
        std::cout << "proposal add/vote/re-vote/tally ok\n";
    }
    {
        FileStore store(path); // reload
        auto all = store.all_proposals();
        assert(all.size() == 1 && all[0].title == "Adopt a flag");
        auto t = store.tally_of(all[0].id);
        assert(t.yes == 1 && t.no == 0 && t.abstain == 1);
        auto acc = store.find_by_username("prop-a");
        assert(store.delete_account(acc->id));
        assert(store.proposal_by_id(all[0].id)->author == "[deleted account]");
        std::cout << "proposal LGP1 tags survive reload; author relabeled on delete\n";
    }

    std::uint64_t reply_post_id = 0;
    {
        FileStore store(path);
        auto a = create_account(store, breach, "reply-a", "correct-horse-battery-staple", "+15559200000");
        auto b = create_account(store, breach, "reply-b", "correct-horse-battery-staple", "+15559200001");
        assert(a.error == AuthError::None && b.error == AuthError::None);
        reply_post_id = store.add_post("reply-a", "a post worth replying to", "");

        auto rid = store.add_reply(reply_post_id, "reply-b", "well said");
        assert(store.replies_of(reply_post_id).size() == 1);
        assert(store.replies_of(reply_post_id)[0].id == rid);
        assert(store.replies_of(reply_post_id)[0].text == "well said");
        assert(store.replies_of(999999).empty());

        assert(!store.has_seconded(reply_post_id, "reply-b"));
        store.toggle_second(reply_post_id, "reply-b");
        assert(store.has_seconded(reply_post_id, "reply-b"));
        assert(store.second_count(reply_post_id) == 1);
        store.toggle_second(reply_post_id, "reply-a");
        assert(store.second_count(reply_post_id) == 2);
        store.toggle_second(reply_post_id, "reply-b"); // toggle off
        assert(!store.has_seconded(reply_post_id, "reply-b"));
        assert(store.second_count(reply_post_id) == 1);
        std::cout << "reply add/list and second toggle/count ok\n";
    }
    {
        FileStore store(path); // reload
        assert(store.replies_of(reply_post_id).size() == 1);
        assert(store.second_count(reply_post_id) == 1);
        assert(store.has_seconded(reply_post_id, "reply-a"));
        auto acc = store.find_by_username("reply-b");
        assert(store.delete_account(acc->id));
        assert(store.replies_of(reply_post_id)[0].author == "[deleted account]");
        std::cout << "reply/second LGP1 tags survive reload; author relabeled, seconds pruned on delete\n";
    }

    unlink(path.c_str());
    unlink((path + ".tmp").c_str());
    std::cout << "\nall file_store tests passed\n";
    return 0;
}
