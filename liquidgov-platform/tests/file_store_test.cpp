#include "liquidgov/platform/auth.hpp"
#include "liquidgov/platform/file_store.hpp"

#include <algorithm>
#include <chrono>
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
        std::cout << "jukebox queued_by, complete, and history ok\n";
    }

    {
        const std::string on_disk = slurp(path);
        assert(on_disk.find("\njt\t") != std::string::npos);
        assert(on_disk.find("\njh\t") != std::string::npos);
        FileStore store(path);
        assert(store.jukebox_of("juke").size() == kJukeboxQueueCap);
        assert(store.jukebox_of("jukeguest").size() == 1);
        assert(store.jukebox_of("jukeguest")[0].queued_by == "jukeguest");
        assert(store.jukebox_history_of("jukeguest").size() == 2);
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

    {
        FileStore store(path);
        BallotSpec num;
        num.kind = BallotKind::Number;
        num.unit = "days";
        num.min_v = 1;
        num.max_v = 365;
        auto nid = store.add_proposal("prop-a", "How many days", "Until the next gathering.", num);
        assert(nid != 0);
        store.cast_ballot_value(nid, "prop-a", "10");
        store.cast_ballot_value(nid, "prop-b", "30");
        store.cast_ballot_value(nid, "prop-a", "20"); // re-vote
        store.cast_ballot_value(nid, "prop-b", "999"); // out of range, ignored
        auto vt = store.value_tally_of(nid);
        assert(vt.n == 2);
        assert(vt.lo == 20 && vt.hi == 30 && vt.median == 20);
        assert(*store.ballot_value_of(nid, "prop-a") == "20");
        assert(*store.ballot_value_of(nid, "prop-b") == "30");

        BallotSpec dt;
        dt.kind = BallotKind::Date;
        auto did = store.add_proposal("prop-a", "By when", "A deadline.", dt);
        store.cast_ballot_value(did, "prop-a", "2027-06-01");
        store.cast_ballot_value(did, "prop-b", "2027-01-15");
        auto dtally = store.value_tally_of(did);
        assert(dtally.n == 2 && dtally.median_date == "2027-01-15");
        assert(dtally.lo_date == "2027-01-15" && dtally.hi_date == "2027-06-01");

        BallotSpec ch;
        ch.kind = BallotKind::Choice;
        ch.options = {"This year", "Next year", "Never"};
        auto cid = store.add_proposal("prop-a", "When to start", "Pick one.", ch);
        store.cast_ballot_value(cid, "prop-a", "0");
        store.cast_ballot_value(cid, "prop-b", "2");
        auto ct = store.value_tally_of(cid);
        assert(ct.option_counts.size() == 3);
        assert(ct.option_counts[0] == 1 && ct.option_counts[2] == 1 && ct.option_counts[1] == 0);

        BallotSpec bad;
        bad.kind = BallotKind::Choice;
        assert(store.add_proposal("prop-a", "nope", "needs two options", bad) == 0);

        FileStore reloaded(path);
        auto np = reloaded.proposal_by_id(nid);
        assert(np && np->ballot.kind == BallotKind::Number && np->ballot.unit == "days");
        assert(reloaded.value_tally_of(nid).n == 2);
        auto cp = reloaded.proposal_by_id(cid);
        assert(cp && cp->ballot.options.size() == 3 && cp->ballot.options[2] == "Never");
        std::cout << "proposal number/date/choice ballots + LGP1 round-trip ok\n";
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

    {
        FileStore store(path);
        auto a = create_account(store, breach, "oc-a", "correct-horse-battery-staple", "+15559200000");
        auto b = create_account(store, breach, "oc-b", "correct-horse-battery-staple", "+15559200001");
        assert(a.error == AuthError::None && b.error == AuthError::None);

        BallotSpec closed;
        closed.kind = BallotKind::Choice;
        closed.options = {"Red", "Blue"};
        auto cid = store.add_proposal("oc-a", "Closed colors", "No write-ins.", closed);
        assert(cid != 0);
        assert(!store.add_proposal_option(cid, "oc-b", "Green"));
        assert(store.add_proposal_option(cid, "oc-a", "Green"));
        assert(store.proposal_by_id(cid)->ballot.options.size() == 3);

        BallotSpec open;
        open.kind = BallotKind::Choice;
        open.open_choice = true;
        open.options = {"Alpha"};
        auto oid = store.add_proposal("oc-a", "Name it", "Add yours.", open);
        assert(oid != 0);
        assert(store.add_proposal_option(oid, "oc-b", "Beta"));
        assert(!store.add_proposal_option(oid, "oc-b", "Gamma")); // second add blocked
        assert(store.add_proposal_option(oid, "oc-a", "Gamma"));  // author unlimited
        assert(!store.add_proposal_option(oid, "oc-b", "beta"));  // duplicate, different case
        auto got = store.proposal_by_id(oid);
        assert(got && got->ballot.open_choice);
        assert(got->ballot.options.size() == 3);
        assert(got->ballot.options[1] == "Beta");
        assert(got->ballot.option_authors[1] == "oc-b");
        assert(got->ballot.option_authors[2].empty());
        const std::string disk = slurp(path);
        assert(disk.find("\npopen\t") != std::string::npos);
        assert(disk.find("\npba\t") != std::string::npos);
        std::cout << "open-choice add/cap/duplicate ok\n";
    }
    {
        FileStore store(path);
        bool found = false;
        for (const auto& p : store.all_proposals()) {
            if (p.title != "Name it") continue;
            found = true;
            assert(p.ballot.open_choice);
            assert(p.ballot.options.size() == 3);
            assert(p.ballot.option_authors.size() == 3);
            assert(p.ballot.option_authors[1] == "oc-b");
        }
        assert(found);
        std::cout << "open-choice popen/pba survive reload\n";
    }

    {
        FileStore store(path);
        auto a = create_account(store, breach, "off-a", "correct-horse-battery-staple", "+15559300000");
        assert(a.error == AuthError::None);
        BallotSpec spec;
        spec.kind = BallotKind::Choice;
        spec.open_choice = true;
        spec.options = {"Ada", "Bea"};
        auto id = store.add_proposal("off-a", "Mayor", "Who should hold the office.", spec);
        assert(id != 0);
        assert(store.mark_proposal_office(id, "Fresno, CA"));
        auto got = store.proposal_by_id(id);
        assert(got && got->is_office && got->jurisdiction == "Fresno, CA");
        const std::string disk = slurp(path);
        assert(disk.find("\npoff\t") != std::string::npos);
        std::cout << "officials mark/poff ok\n";
    }
    {
        FileStore store(path);
        bool found = false;
        for (const auto& p : store.all_proposals()) {
            if (p.title != "Mayor") continue;
            found = true;
            assert(p.is_office);
            assert(p.jurisdiction == "Fresno, CA");
        }
        assert(found);
        std::cout << "officials poff survives reload\n";
    }
    {
        FileStore store(path);
        assert(!store.jukebox_pos("site"));
        store.set_jukebox_pos("site", "abcdefghijk", 100, 50);
        auto p = store.jukebox_pos("site");
        assert(p && p->video_id == "abcdefghijk" && p->offset_sec == 100);
        store.clear_jukebox_pos("site");
        assert(!store.jukebox_pos("site"));
        store.clear_jukebox_pos("site");
        std::cout << "jpos set/clear ok\n";
    }
    {
        FileStore store(path);
        assert(!store.jukebox_pos("site"));
        std::cout << "jpos clear survives reload\n";
    }

    // City on groups (city proposals build plan, slice 1).
    {
        assert(clean_city("  Fresno,\t CA \n") == "Fresno, CA");
        assert(clean_city("a\x01" "b") == "ab");
        assert(clean_city(" \t ") == "");
        const std::string long_city = std::string(79, 'x') + "\xC3\xA9" + "yyy"; // é straddles byte 80
        assert(clean_city(long_city) == std::string(79, 'x'));
        assert(city_key("Fresno, CA") == "fresno-ca");
        assert(city_key("  fresno   ca ") == "fresno-ca");
        assert(city_key("FRESNO.CA") == "fresno-ca");
        assert(city_key("S\xC3\xA3o Paulo") == "s\xC3\xA3o-paulo");
        assert(city_key(", . -") == "");
        assert(city_key("") == "");
        std::cout << "clean_city / city_key ok\n";

        const std::string cpath = tmp_path();
        {
            FileStore store(cpath);
            auto mk = [&](const std::string& slug, const std::string& name, const std::string& owner, const std::string& city) {
                Community c;
                c.slug = slug; c.name = name; c.owner = owner; c.created_at = 1;
                c.category = CommunityCategory::LocalCivic; c.subcategory = "Neighbors";
                c.city = city;
                return store.create_community(c);
            };
            assert(mk("fres1", "Fresno Neighbors", "ann", "  Fresno,  CA "));
            assert(mk("fres2", "Fresno Gardens", "bob", "fresno ca"));
            assert(mk("fres3", "Tower District", "cat", "Fresno, CA"));
            assert(mk("oak1", "Oakland Bikes", "ann", "Oakland, CA"));
            assert(mk("nocity", "No City", "ann", ""));
            assert(mk("junk", "Junk City", "ann", " ,. "));
            assert(store.community("fres1")->city == "Fresno, CA");
            assert(store.community("junk")->city.empty());
            assert(store.community("nocity")->city.empty());

            auto cities = store.cities_in_use();
            assert(cities.size() == 2);
            assert(cities[0].key == "fresno-ca" && cities[0].name == "Fresno, CA" && cities[0].groups == 3);
            assert(cities[1].key == "oakland-ca" && cities[1].groups == 1);

            auto fres = store.communities_in_city("fresno-ca");
            assert(fres.size() == 3);
            assert(fres[0].slug == "fres2" && fres[1].slug == "fres1" && fres[2].slug == "fres3"); // by name
            assert(store.communities_in_city("oakland-ca").size() == 1);
            assert(store.communities_in_city("nowhere").empty());
            assert(store.communities_in_city("").empty());

            // Only the owner sets or clears it.
            assert(!store.set_community_city("oak1", "bob", "Fresno, CA"));
            assert(store.community("oak1")->city == "Oakland, CA");
            assert(!store.set_community_city("missing", "ann", "Fresno, CA"));
            assert(store.set_community_city("nocity", "ann", "Oakland,CA"));
            assert(store.communities_in_city("oakland-ca").size() == 2);
            assert(store.set_community_city("nocity", "ann", "  "));
            assert(store.community("nocity")->city.empty());
            assert(store.communities_in_city("oakland-ca").size() == 1);
            std::cout << "city on groups: create, list, owner-only set/clear ok\n";
        }
        {
            const std::string disk = slurp(cpath);
            assert(disk.find("ccity\tfres1\tFresno, CA\n") != std::string::npos);
            assert(disk.find("ccity\tnocity\t") == std::string::npos);
            assert(disk.find("ccity\tjunk\t") == std::string::npos);
            FileStore store(cpath);
            assert(store.community("fres2")->city == "fresno ca");
            assert(store.community("oak1")->city == "Oakland, CA");
            assert(store.community("nocity")->city.empty());
            assert(store.communities_in_city("fresno-ca").size() == 3);
            // Deleting a group takes its city with it, and no orphan ccity is written.
            assert(store.delete_community("oak1", "ann"));
            assert(store.cities_in_use().size() == 1);
        }
        {
            const std::string disk = slurp(cpath);
            assert(disk.find("ccity\toak1\t") == std::string::npos);
            FileStore store(cpath);
            assert(store.cities_in_use().size() == 1);
            std::cout << "city survives reload; delete leaves no orphan\n";
        }
        unlink(cpath.c_str());
        unlink((cpath + ".tmp").c_str());

        // A store written before cities existed loads unchanged; ccity may come
        // before its comm; a ccity for a missing group is dropped.
        const std::string old = tmp_path();
        {
            std::ofstream out(old);
            out << "LGP1\n"
                << "ccity\tlater\tFresno%2C CA\n"
                << "comm\told\tOld group\tann\tabout\t5\n"
                << "comm\tnewer\tNewer\tann\tabout\t6\t0\tNeighbors\n"
                << "comm\tlater\tLater\tann\tabout\t7\t0\tNeighbors\n"
                << "ccity\tghost\tNowhere\n";
        }
        {
            FileStore store(old);
            assert(store.community("old").has_value());
            assert(store.community("old")->city.empty());
            assert(store.community("old")->category == CommunityCategory::General);
            assert(store.community("newer")->city.empty());
            assert(store.community("later")->city == "Fresno, CA");
            assert(!store.community("ghost").has_value());
            assert(store.cities_in_use().size() == 1);
            std::cout << "old store loads; ccity order-free; orphan dropped\n";
        }
        unlink(old.c_str());
        unlink((old + ".tmp").c_str());

        const std::string bad = tmp_path();
        {
            std::ofstream out(bad);
            out << "LGP1\nccity\tonlyslug\n";
        }
        bool threw = false;
        try {
            FileStore store(bad);
        } catch (const std::runtime_error& e) {
            threw = std::string(e.what()).find("bad ccity") != std::string::npos;
        }
        assert(threw);
        unlink(bad.c_str());
        std::cout << "malformed ccity is fail-closed\n";

        // People here (slice 2): opt in per city. An unlisted member stays hidden.
        const std::string ppath = tmp_path();
        {
            FileStore store(ppath);
            auto mk = [&](const std::string& slug, const std::string& owner, const std::string& city) {
                Community c;
                c.slug = slug; c.name = slug; c.owner = owner; c.created_at = 1;
                c.category = CommunityCategory::LocalCivic; c.subcategory = "Neighbors";
                c.city = city;
                return store.create_community(c);
            };
            assert(mk("fres1", "ann", "Fresno, CA"));
            assert(mk("oak1", "bob", "Oakland, CA"));
            assert(store.join_community("fres1", "bob"));
            assert(store.people_listed_in_city("fresno-ca").empty());
            assert(!store.set_city_listed("fresno-ca", "nope", true));
            assert(!store.set_city_listed("oakland-ca", "ann", true));
            assert(store.set_city_listed("fresno-ca", "ann", true));
            assert(store.set_city_listed("fresno-ca", "bob", true));
            auto listed = store.people_listed_in_city("fresno-ca");
            assert(listed.size() == 2 && listed[0] == "ann" && listed[1] == "bob");
            assert(store.people_listed_in_city("oakland-ca").empty());
            assert(store.is_city_listed("fresno-ca", "ann"));
            assert(!store.is_city_listed("oakland-ca", "ann"));
            assert(store.set_city_listed("fresno-ca", "bob", false));
            assert(store.people_listed_in_city("fresno-ca").size() == 1);
            store.leave_community("fres1", "bob");
            assert(!store.is_city_listed("fresno-ca", "bob"));
            std::cout << "city list: unlisted hidden, per city, leave drops it\n";
        }
        {
            const std::string disk = slurp(ppath);
            assert(disk.find("clist\tfresno-ca\tann\n") != std::string::npos);
            assert(disk.find("clist\tfresno-ca\tbob\n") == std::string::npos);
            FileStore store(ppath);
            assert(store.people_listed_in_city("fresno-ca").size() == 1);
            assert(store.people_listed_in_city("fresno-ca")[0] == "ann");
            std::cout << "city list survives reload\n";
        }
        unlink(ppath.c_str());
        unlink((ppath + ".tmp").c_str());

        const std::string listed_old = tmp_path();
        {
            std::ofstream out(listed_old);
            out << "LGP1\n"
                << "clist\tfresno-ca\tann\n"
                << "comm\tfres1\tFresno\tann\tabout\t1\t0\tNeighbors\n"
                << "cm\tfres1\tann\n"
                << "ccity\tfres1\tFresno, CA\n"
                << "clist\tfresno-ca\tghost\n"
                << "clist\toakland-ca\tann\n";
        }
        {
            FileStore store(listed_old);
            auto listed = store.people_listed_in_city("fresno-ca");
            assert(listed.size() == 1 && listed[0] == "ann");
            assert(store.people_listed_in_city("oakland-ca").empty());
            std::cout << "clist order-free; orphan and other-city rows dropped\n";
        }
        unlink(listed_old.c_str());
        {
            std::ofstream out(listed_old);
            out << "LGP1\nclist\tonlykey\n";
        }
        bool list_threw = false;
        try {
            FileStore store(listed_old);
        } catch (const std::runtime_error& e) {
            list_threw = std::string(e.what()).find("bad clist") != std::string::npos;
        }
        assert(list_threw);
        unlink(listed_old.c_str());
        std::cout << "malformed clist is fail-closed\n";
    }

    // Slice 3: a proposal can name a city groups already use.
    {
        const std::string ppath = tmp_path();
        std::uint64_t park_id = 0;
        std::uint64_t race_id = 0;
        {
            FileStore store(ppath);
            Community c;
            c.slug = "fres1"; c.name = "Fresno"; c.owner = "ann"; c.created_at = 1;
            c.category = CommunityCategory::LocalCivic; c.subcategory = "Neighbors";
            c.city = "Fresno, CA";
            assert(store.create_community(c));
            park_id = store.add_proposal("ann", "Fix the park", "More trees.");
            assert(park_id != 0);
            assert(!store.set_proposal_city(park_id, "Nowhere"));
            assert(store.proposal_by_id(park_id)->city.empty());
            assert(store.set_proposal_city(park_id, "Fresno, CA"));
            assert(store.proposal_by_id(park_id)->city == "fresno-ca");
            assert(store.set_proposal_actor(park_id, "  The city council  "));
            auto got = store.proposal_by_id(park_id);
            assert(got && got->who_acts == "The city council");
            assert(!got->is_office && got->jurisdiction.empty());
            assert(store.proposals_in_city("fresno-ca").size() == 1);
            assert(store.proposals_in_city("oakland-ca").empty());
            BallotSpec spec;
            spec.kind = BallotKind::Choice;
            spec.options = {"Ada", "Bea"};
            race_id = store.add_proposal("ann", "Mayor", "Who should hold the office.", spec);
            assert(race_id != 0);
            assert(store.mark_proposal_office(race_id, "Fresno, CA"));
            assert(store.set_proposal_city(race_id, "Fresno, CA"));
            auto off = store.proposal_by_id(race_id);
            assert(off && off->is_office && off->jurisdiction == "Fresno, CA" && off->city == "fresno-ca");
            std::cout << "proposal city: only a city in use, officials place stays\n";
        }
        {
            const std::string disk = slurp(ppath);
            assert(disk.find("pcity\t" + std::to_string(park_id) + "\tfresno-ca\n") != std::string::npos);
            assert(disk.find("pwho\t" + std::to_string(park_id) + "\tThe city council\n") != std::string::npos);
            assert(disk.find("poff\t" + std::to_string(race_id) + "\tFresno, CA\n") != std::string::npos);
            FileStore store(ppath);
            auto got = store.proposal_by_id(park_id);
            assert(got && got->city == "fresno-ca" && got->who_acts == "The city council");
            auto off = store.proposal_by_id(race_id);
            assert(off && off->is_office && off->jurisdiction == "Fresno, CA" && off->city == "fresno-ca");
            // The last group leaving keeps the proposal's city (Brandon 2026-11-06, "keep it").
            assert(store.delete_community("fres1", "ann"));
            assert(store.proposal_by_id(park_id)->city == "fresno-ca");
            assert(store.proposal_by_id(park_id)->who_acts == "The city council");
            const std::string after = slurp(ppath);
            assert(after.find("pcity\t" + std::to_string(park_id) + "\tfresno-ca\n") != std::string::npos);
            assert(after.find("pwho\t" + std::to_string(park_id) + "\tThe city council\n") != std::string::npos);
        }
        {
            FileStore store(ppath);
            assert(store.proposal_by_id(park_id)->city == "fresno-ca");
            assert(store.proposals_in_city("fresno-ca").size() == 2);
            std::cout << "proposal city survives reload and its city's last group leaving\n";
        }
        unlink(ppath.c_str());
        unlink((ppath + ".tmp").c_str());

        const std::string hand = tmp_path();
        {
            std::ofstream out(hand);
            out << "LGP1\n"
                << "comm\tfres1\tFresno\tann\tabout\t1\t0\tNeighbors\n"
                << "cm\tfres1\tann\n"
                << "ccity\tfres1\tFresno, CA\n"
                << "pcity\t1\tfresno-ca\n"
                << "prop\t1\tann\t1\tPark\tTrees\n"
                << "pwho\t1\tThe council\n"
                << "prop\t2\tann\t1\tOther\tNope\n"
                << "pcity\t2\toakland-ca\n"
                << "pcity\t99\tfresno-ca\n";
        }
        {
            FileStore store(hand);
            auto a = store.proposal_by_id(1);
            assert(a && a->city == "fresno-ca" && a->who_acts == "The council");
            auto b = store.proposal_by_id(2);
            assert(b && b->city == "oakland-ca"); // kept though no group names Oakland
            assert(!store.proposal_by_id(99));
            std::cout << "pcity order-free; groupless city kept; orphan row dropped\n";
        }
        unlink(hand.c_str());
        {
            std::ofstream out(hand);
            out << "LGP1\npcity\tonly\n";
        }
        bool pcity_threw = false;
        try { FileStore store(hand); } catch (const std::runtime_error& e) {
            pcity_threw = std::string(e.what()).find("bad pcity") != std::string::npos;
        }
        assert(pcity_threw);
        unlink(hand.c_str());
        {
            std::ofstream out(hand);
            out << "LGP1\npwho\tonly\n";
        }
        bool pwho_threw = false;
        try { FileStore store(hand); } catch (const std::runtime_error& e) {
            pwho_threw = std::string(e.what()).find("bad pwho") != std::string::npos;
        }
        assert(pwho_threw);
        unlink(hand.c_str());
        std::cout << "malformed pcity and pwho are fail-closed\n";
    }

    // Slice 4: departments and councils, per city, picked by a proposal.
    {
        const std::string tpath = tmp_path();
        std::uint64_t park_id = 0;
        {
            FileStore store(tpath);
            Community c;
            c.slug = "fres1"; c.name = "Fresno"; c.owner = "ann"; c.created_at = 1;
            c.category = CommunityCategory::LocalCivic; c.subcategory = "Neighbors";
            c.city = "Fresno, CA";
            assert(store.create_community(c));
            assert(!store.add_city_tag("fresno-ca", "bob", "Parks"));
            assert(!store.add_city_tag("fresno-ca", "ann", "   "));
            assert(store.add_city_tag("fresno-ca", "ann", "  Parks Department "));
            assert(store.add_city_tag("fresno-ca", "ann", "parks department"));
            auto tags = store.city_tags("fresno-ca");
            assert(tags.size() == 1 && tags[0] == "Parks Department");
            assert(store.add_city_tag("fresno-ca", "ann", std::string(81, 'x')));
            tags = store.city_tags("fresno-ca");
            assert(tags.size() == 2 && tags[1] == std::string(80, 'x'));
            assert(!store.add_city_tag("oakland-ca", "ann", "Council"));
            park_id = store.add_proposal("ann", "Fix the park", "More trees.");
            assert(park_id != 0);
            assert(store.set_proposal_city(park_id, "Fresno, CA"));
            assert(store.set_proposal_tags(park_id, {"parks department", "Nope"}));
            auto got = store.proposal_by_id(park_id);
            assert(got && got->tags.size() == 1 && got->tags[0] == "Parks Department");
            auto plain = store.add_proposal("ann", "Benches", "More seats.");
            assert(store.set_proposal_tags(plain, {"Parks Department"}));
            assert(store.proposal_by_id(plain)->tags.empty());
            std::cout << "city tags: members only, case-folded, proposal keeps its city's\n";
        }
        {
            const std::string disk = slurp(tpath);
            assert(disk.find("ctag\tfresno-ca\tParks Department\n") != std::string::npos);
            assert(disk.find("ctag\tfresno-ca\tparks department\n") == std::string::npos);
            assert(disk.find("ptag\t" + std::to_string(park_id) + "\tParks Department\n") != std::string::npos);
            assert(disk.find("Nope") == std::string::npos);
            FileStore store(tpath);
            assert(store.city_tags("fresno-ca").size() == 2);
            assert(store.proposal_by_id(park_id)->tags.size() == 1);
            // The last group leaving keeps the city's tags and the proposal's picks.
            assert(store.delete_community("fres1", "ann"));
            assert(store.city_tags("fresno-ca").size() == 2);
            assert(store.proposal_by_id(park_id)->city == "fresno-ca");
            assert(store.proposal_by_id(park_id)->tags.size() == 1);
            const std::string after = slurp(tpath);
            assert(after.find("\nctag\tfresno-ca\tParks Department\n") != std::string::npos);
            assert(after.find("\nptag\t" + std::to_string(park_id) + "\tParks Department\n") != std::string::npos);
        }
        {
            // A new group naming the city again finds them all still there.
            FileStore store(tpath);
            assert(store.city_tags("fresno-ca").size() == 2);
            assert(store.proposal_by_id(park_id)->tags.size() == 1);
            Community c;
            c.slug = "fres2"; c.name = "Fresno again"; c.owner = "bob"; c.created_at = 2;
            c.category = CommunityCategory::LocalCivic; c.subcategory = "Neighbors";
            c.city = "fresno ca";
            assert(store.create_community(c));
            assert(store.proposals_in_city("fresno-ca").size() == 1);
            assert(store.add_city_tag("fresno-ca", "bob", "Council"));
            assert(store.city_tags("fresno-ca").size() == 3);
            std::cout << "city tags survive reload and the last group leaving; they return with a new group\n";
        }
        unlink(tpath.c_str());
        unlink((tpath + ".tmp").c_str());

        const std::string hand = tmp_path();
        {
            std::ofstream out(hand);
            out << "LGP1\n"
                << "ctag\tfresno-ca\tParks\n"
                << "comm\tfres1\tFresno\tann\tabout\t1\t0\tNeighbors\n"
                << "cm\tfres1\tann\n"
                << "ccity\tfres1\tFresno, CA\n"
                << "prop\t1\tann\t1\tPark\tTrees\n"
                << "pcity\t1\tfresno-ca\n"
                << "ptag\t1\tparks\n"
                << "ptag\t1\tNope\n"
                << "ctag\tfresno-ca\tparks\n"
                << "ctag\toakland-ca\tCouncil\n";
        }
        {
            FileStore store(hand);
            auto tags = store.city_tags("fresno-ca");
            assert(tags.size() == 1 && tags[0] == "Parks");
            assert(store.city_tags("oakland-ca").size() == 1); // kept though no group names Oakland
            auto p = store.proposal_by_id(1);
            assert(p && p->tags.size() == 1 && p->tags[0] == "Parks");
            std::cout << "ctag order-free; case duplicate and unknown ptag dropped; groupless city kept\n";
        }
        unlink(hand.c_str());
        {
            std::ofstream out(hand);
            out << "LGP1\nctag\tonly\n";
        }
        bool ctag_threw = false;
        try { FileStore store(hand); } catch (const std::runtime_error& e) {
            ctag_threw = std::string(e.what()).find("bad ctag") != std::string::npos;
        }
        assert(ctag_threw);
        unlink(hand.c_str());
        {
            std::ofstream out(hand);
            out << "LGP1\nptag\tonly\n";
        }
        bool ptag_threw = false;
        try { FileStore store(hand); } catch (const std::runtime_error& e) {
            ptag_threw = std::string(e.what()).find("bad ptag") != std::string::npos;
        }
        assert(ptag_threw);
        unlink(hand.c_str());
        std::cout << "malformed ctag and ptag are fail-closed\n";
    }

    // Slice 5: comments and one reaction per person on a proposal.
    {
        const std::string tpath = tmp_path();
        std::uint64_t pid = 0;
        std::uint64_t rid = 0;
        {
            FileStore store(tpath);
            auto ann = create_account(store, breach, "ann", "correct-horse-battery-staple", "+15559210000");
            auto bob = create_account(store, breach, "bob", "correct-horse-battery-staple", "+15559210001");
            assert(ann.error == AuthError::None && bob.error == AuthError::None);
            pid = store.add_proposal("ann", "Plant trees", "More shade.");
            assert(store.add_proposal_reply(pid, "bob", "   ") == 0);
            assert(store.add_proposal_reply(99, "bob", "no such proposal") == 0);
            rid = store.add_proposal_reply(pid, "bob", "  Shade helps.\nReally.  ");
            assert(rid != 0);
            auto replies = store.proposal_replies_of(pid);
            assert(replies.size() == 1 && replies[0].text == "Shade helps.\nReally.");
            assert(store.add_proposal_reply(pid, "ann", std::string(1001, 'x')) != 0);
            replies = store.proposal_replies_of(pid);
            assert(replies.size() == 2 && replies[1].text.size() == 1000);
            assert(!store.set_proposal_reaction(pid, "bob", "love"));
            assert(store.proposal_reaction_of(pid, "bob") == std::nullopt);
            assert(store.set_proposal_reaction(pid, "bob", "support"));
            assert(store.set_proposal_reaction(pid, "bob", "oppose"));
            assert(store.proposal_reaction_of(pid, "bob") == "oppose");
            assert(store.set_proposal_reaction(pid, "ann", "support"));
            auto counts = store.proposal_reaction_counts(pid);
            assert(counts.size() == 4 && counts[0].first == "support" && counts[0].second == 1);
            assert(counts[1].first == "oppose" && counts[1].second == 1);
            assert(store.set_proposal_reaction(pid, "bob", ""));
            assert(store.proposal_reaction_of(pid, "bob") == std::nullopt);
            assert(!store.set_proposal_reaction(99, "bob", "support"));
            std::cout << "proposal comments and reactions: cap, one per person, change, remove\n";
        }
        {
            const std::string disk = slurp(tpath);
            assert(disk.find("prply\t" + std::to_string(rid) + "\t" + std::to_string(pid) + "\tbob\t") != std::string::npos);
            assert(disk.find("prx\t" + std::to_string(pid) + "\tbob\t") == std::string::npos);
            assert(disk.find("prx\t" + std::to_string(pid) + "\tann\tsupport\n") != std::string::npos);
            FileStore store(tpath);
            assert(store.proposal_replies_of(pid).size() == 2);
            assert(store.proposal_reaction_of(pid, "ann") == "support");
            assert(store.delete_account(store.find_by_username("bob")->id));
            auto replies = store.proposal_replies_of(pid);
            assert(replies[0].author == "[deleted account]");
            assert(store.proposal_reaction_of(pid, "ann") == "support");
            std::cout << "proposal comments survive reload; a deleted commenter is relabeled\n";
        }
        unlink(tpath.c_str());
        unlink((tpath + ".tmp").c_str());

        const std::string hand = tmp_path();
        {
            std::ofstream out(hand);
            out << "LGP1\n"
                << "prop\t1\tann\t1\tPark\tTrees\n"
                << "prx\t1\tbob\tlove\n"
                << "prx\t1\tbob\tsupport\n"
                << "prx\t9\tann\toppose\n"
                << "prply\t4\t1\t\t1\tblank author\n"
                << "prply\t3\t9\tbob\t1\torphan\n"
                << "prply\t2\t1\tbob\t1\tsecond\n"
                << "prply\t1\t1\tann\t1\tfirst\n";
        }
        {
            FileStore store(hand);
            auto replies = store.proposal_replies_of(1);
            assert(replies.size() == 2 && replies[0].text == "second" && replies[1].text == "first");
            assert(store.proposal_reaction_of(1, "bob") == "support");
            assert(store.proposal_reaction_counts(9)[1].second == 0);
            std::cout << "prply order kept; bad reaction, orphan comment, and blank author dropped\n";
        }
        unlink(hand.c_str());
        {
            std::ofstream out(hand);
            out << "LGP1\nprply\tonly\n";
        }
        bool prply_threw = false;
        try { FileStore store(hand); } catch (const std::runtime_error& e) {
            prply_threw = std::string(e.what()).find("bad prply") != std::string::npos;
        }
        assert(prply_threw);
        unlink(hand.c_str());
        {
            std::ofstream out(hand);
            out << "LGP1\nprx\tonly\n";
        }
        bool prx_threw = false;
        try { FileStore store(hand); } catch (const std::runtime_error& e) {
            prx_threw = std::string(e.what()).find("bad prx") != std::string::npos;
        }
        assert(prx_threw);
        unlink(hand.c_str());
        std::cout << "malformed prply and prx are fail-closed\n";
    }

    // Map privacy settings (slice 6): defaults, save, reload, fail-closed.
    {
        const std::string mpath = tmp_path();
        {
            FileStore store(mpath);
            MapPrefs d = store.map_prefs_of("ann");
            assert(d.show && d.groups && d.posts && d.comments && !d.friends); // friends opt-in
            MapPrefs mp;
            mp.show = true; mp.groups = false; mp.posts = true; mp.comments = false; mp.friends = true;
            store.set_map_prefs("ann", mp);
            store.set_map_prefs("", mp); // ignored
            auto got = store.map_prefs_of("ann");
            assert(got.show && !got.groups && got.posts && !got.comments && got.friends);
            assert(store.map_prefs_of("bob").show && !store.map_prefs_of("bob").friends);
        }
        {
            const std::string disk = slurp(mpath);
            assert(disk.find("mapp\tann\t1\t0\t1\t0\t1\t1\n") != std::string::npos); // 8th field: pains (default on)
            assert(disk.find("mapp\tbob") == std::string::npos); // defaults are not written
            assert(disk.find("mapp\t\t") == std::string::npos);
            FileStore store(mpath);
            auto got = store.map_prefs_of("ann");
            assert(got.show && !got.groups && got.posts && !got.comments && got.friends);
            std::cout << "map prefs: defaults (friends off), save, reload ok\n";
        }
        unlink(mpath.c_str());
        unlink((mpath + ".tmp").c_str());
        // Deleting an account drops its map settings.
        {
            const std::string dpath = tmp_path();
            {
                FileStore store(dpath);
                auto r = create_account(store, breach, "maptest", "correct-horse-battery-staple", "+15557770000");
                assert(r.error == AuthError::None);
                MapPrefs off; off.show = false;
                store.set_map_prefs("maptest", off);
                assert(!store.map_prefs_of("maptest").show);
                auto acc = store.find_by_username("maptest");
                assert(acc && store.delete_account(acc->id));
                assert(store.map_prefs_of("maptest").show); // back to defaults
            }
            assert(slurp(dpath).find("mapp\tmaptest") == std::string::npos);
            unlink(dpath.c_str());
            unlink((dpath + ".tmp").c_str());
            std::cout << "map prefs: deleting the account drops them\n";
        }
        for (const char* badline : {"LGP1\nmapp\tann\t1\t1\t1\t1\n", "LGP1\nmapp\tann\t1\t1\t1\t1\t2\n"}) {
            const std::string bad = tmp_path();
            { std::ofstream out(bad); out << badline; }
            bool threw = false;
            try { FileStore store(bad); } catch (const std::runtime_error& e) {
                threw = std::string(e.what()).find("bad mapp") != std::string::npos;
            }
            assert(threw);
            unlink(bad.c_str());
        }
        std::cout << "malformed mapp is fail-closed\n";
    }

    // Wishes and pain points: the detector (pure function).
    {
        auto one = [](const std::string& text) { return find_pain_points(text); };
        auto s = one("I really wish the buses ran later on weekends. Anyway, see you!");
        assert(!s.crisis && s.hits.size() == 1 && s.hits[0].kind == "wish" && s.hits[0].phrase == "the buses ran later on weekends");
        s = one("I'm so tired of waiting for the light at Shields");
        assert(s.hits.size() == 1 && s.hits[0].kind == "pain" && s.hits[0].phrase == "waiting for the light at Shields");
        s = one("i hate when the potholes on Olive wreck my bike");
        assert(s.hits.size() == 1 && s.hits[0].kind == "pain" && s.hits[0].phrase == "the potholes on Olive wreck my bike");
        s = one("I hate it when the park closes early!");
        assert(s.hits.size() == 1 && s.hits[0].phrase == "the park closes early");
        s = one("I\xE2\x80\x99m tired of paying for parking downtown");
        assert(s.hits.size() == 1 && s.hits[0].kind == "pain" && s.hits[0].phrase == "paying for parking downtown");
        s = one("If only the library opened on Sundays");
        assert(s.hits.size() == 1 && s.hits[0].kind == "wish");
        s = one("It would be great if Blackstone had shade trees");
        assert(s.hits.size() == 1 && s.hits[0].phrase == "Blackstone had shade trees");
        // Cut at 12 words.
        s = one("I wish one two three four five six seven eight nine ten eleven twelve thirteen fourteen");
        assert(s.hits.size() == 1 && s.hits[0].phrase == "one two three four five six seven eight nine ten eleven twelve");
        // Not triggers on purpose: debate and sarcasm.
        assert(one("Someone should fix the roads. Why can't the city do anything?").hits.empty());
        // Greetings, no subject, inside quotes, quoted lines, inside other words.
        assert(one("I wish you a happy birthday!").hits.empty());
        assert(one("I wish.").hits.empty());
        assert(one("She said \"I hate when the buses are late\" and laughed").hits.empty());
        assert(one("> I hate when the buses are late\nlol same").hits.empty());
        assert(one("Swish! I wishbone the turkey").hits.empty());
        // At most 3, no duplicates.
        s = one("I wish parks were open. I wish parks were open. I hate when roads flood. I'm tired of noise downtown. If only libraries stayed open.");
        assert(s.hits.size() == 3);
        // Crisis words: nothing kept at all.
        for (const char* c : {"I wish I wasn't here anymore", "honestly I want to die", "I'm tired of everything, I wish I were dead",
                              "I\xE2\x80\x99m thinking about suicide"}) {
            auto cs = one(c);
            assert(cs.crisis && cs.hits.empty());
        }
        // Keywords and overlap.
        auto kw = pain_keywords("the buses ran later on weekends");
        assert(std::find(kw.begin(), kw.end(), "buses") != kw.end() && std::find(kw.begin(), kw.end(), "the") == kw.end());
        assert(pain_words_overlap(kw, "Fresno Area Express (bus)"));
        assert(pain_words_overlap(pain_keywords("shade trees on Blackstone"), "Plant a tree on every block"));
        assert(!pain_words_overlap(kw, "Parks Department"));
        assert(!pain_words_overlap({}, "anything"));
        std::cout << "pain points: detector ok (triggers, cuts, skips, crisis, keywords)\n";
    }
    // Wishes and pain points: the store.
    {
        const std::string ppath = tmp_path();
        std::uint64_t kept_id = 0, sugg_id = 0, post_id = 0;
        {
            FileStore store(ppath);
            assert(create_account(store, breach, "ann", "correct-horse-battery-staple", "+15558880001").error == AuthError::None);
            assert(create_account(store, breach, "bob", "correct-horse-battery-staple", "+15558880002").error == AuthError::None);
            post_id = store.add_post("ann", "I wish the buses ran later", "");
            const std::string src = "post:" + std::to_string(post_id);
            kept_id = store.add_pain_suggestion("ann", {"wish", "the buses ran later"}, src);
            assert(kept_id != 0);
            assert(store.add_pain_suggestion("ann", {"wish", "The Buses ran later"}, src) == 0); // same phrase+source
            assert(store.add_pain_suggestion("ann", {"other", "x y z"}, src) == 0);              // bad kind
            assert(store.add_pain_suggestion("", {"wish", "x"}, src) == 0);
            sugg_id = store.add_pain_suggestion("ann", {"pain", "parking downtown"}, src);
            assert(sugg_id != 0);
            assert(!store.keep_pain("bob", kept_id));    // not bob's
            assert(!store.forget_pain("bob", kept_id));
            assert(store.keep_pain("ann", kept_id));
            auto mine = store.pains_of("ann");
            assert(mine.size() == 2 && mine[0].id == sugg_id && !mine[0].kept && mine[1].kept);
            assert(store.pains_of("bob").empty());
        }
        {
            const std::string disk = slurp(ppath);
            assert(disk.find("pain\t" + std::to_string(kept_id) + "\tann\twish\t1\t") != std::string::npos);
            // Only the short phrase is stored with the pain row, never the post text.
            assert(disk.find("\tpost:" + std::to_string(post_id) + "\tthe buses ran later\n") != std::string::npos);
            FileStore store(ppath);
            assert(store.pains_of("ann").size() == 2);
            assert(store.pain_by_id(kept_id)->kept && store.pain_by_id(kept_id)->phrase == "the buses ran later");
            // Forget one; the next id never reuses it.
            assert(store.forget_pain("ann", sugg_id));
            assert(!store.pain_by_id(sugg_id));
            const auto next = store.add_pain_suggestion("ann", {"pain", "noise at night"}, "post:" + std::to_string(post_id));
            assert(next > sugg_id);
            // Turning the switch off deletes everything.
            MapPrefs off; off.pains = false;
            store.set_map_prefs("ann", off);
            assert(store.pains_of("ann").empty());
            MapPrefs on;
            store.set_map_prefs("ann", on);
            // Deleting the source post removes what came from it.
            const auto a2 = store.add_pain_suggestion("ann", {"wish", "the buses ran later"}, "post:" + std::to_string(post_id));
            assert(a2 != 0 && store.keep_pain("ann", a2));
            assert(store.delete_post(post_id, "ann"));
            assert(store.pains_of("ann").empty());
            // A missing source is never kept, even if added.
            store.add_pain_suggestion("ann", {"wish", "something about roads"}, "post:999999");
            assert(store.pains_of("ann").empty());
        }
        {
            FileStore store(ppath);
            assert(store.pains_of("ann").empty());
            const auto pid = store.add_proposal("ann", "Fix roads", "body");
            const auto a3 = store.add_pain_suggestion("ann", {"pain", "potholes everywhere"}, "prop:" + std::to_string(pid));
            assert(a3 != 0);
            auto acc = store.find_by_username("ann");
            assert(acc && store.delete_account(acc->id));
            assert(store.pains_of("ann").empty());
        }
        {
            const std::string disk = slurp(ppath);
            assert(disk.find("\tann\twish\t") == std::string::npos && disk.find("\tann\tpain\t") == std::string::npos);
        }
        unlink(ppath.c_str());
        unlink((ppath + ".tmp").c_str());

        // Private prefixes survive the same flush that keeps post:. A missing
        // post, an unknown prefix, the other person's mail, someone else's
        // group message, and a deleted group message do not.
        {
            const std::string priv = tmp_path();
            std::uint64_t fpid = 0, gpid = 0, mpid = 0, gmid = 0;
            {
                FileStore store(priv);
                assert(create_account(store, breach, "ann", "correct-horse-battery-staple", "+15558880021").error == AuthError::None);
                assert(create_account(store, breach, "bob", "correct-horse-battery-staple", "+15558880022").error == AuthError::None);
                const auto fid = store.add_post("ann", "friends only wish", "");
                const auto gid = store.add_post("ann", "group wish", "fres1");
                fpid = store.add_pain_suggestion("ann", {"wish", "the library stayed open"}, "fpost:" + std::to_string(fid));
                gpid = store.add_pain_suggestion("ann", {"pain", "the buses skip my stop"}, "gpost:" + std::to_string(gid));
                assert(fpid != 0 && store.pain_by_id(fpid));
                assert(gpid != 0 && store.pain_by_id(gpid));
                store.add_mail("ann", "bob", "sidewalks");
                const auto sent = store.mail_sent_by("ann");
                assert(sent.size() == 1);
                mpid = store.add_pain_suggestion("ann", {"pain", "the sidewalks flood"},
                                                 "mail:" + std::to_string(sent[0].ts) + ":bob");
                assert(mpid != 0 && store.pain_by_id(mpid));
                store.add_friend_request("ann", "bob");
                assert(store.accept_friend_request("ann", "bob"));
                auto ann = store.find_by_username("ann");
                auto bob = store.find_by_username("bob");
                TestSocialGraph graph(store);
                auto created = create_conversation(store, graph, ann->id, {bob->id}, true);
                assert(created.error == MessengerError::None && created.conversation_id);
                auto sent_g = send_message(store, ann->id, *created.conversation_id, "pool earlier");
                assert(sent_g.error == MessengerError::None && sent_g.message_id);
                gmid = store.add_pain_suggestion("ann", {"wish", "the pool opened earlier"},
                                                 "gmsg:" + std::to_string(*sent_g.message_id));
                assert(gmid != 0 && store.pain_by_id(gmid));
                assert(store.pains_of("ann").size() == 4);
                store.add_pain_suggestion("ann", {"wish", "unknown prefix"}, "nope:1");
                store.add_pain_suggestion("ann", {"wish", "missing friends post"}, "fpost:999999");
                store.add_mail("bob", "ann", "trains");
                const auto inbound = store.mail_sent_by("bob");
                assert(inbound.size() == 1);
                store.add_pain_suggestion("ann", {"wish", "the trains ran all night"},
                                          "mail:" + std::to_string(inbound[0].ts) + ":ann");
                auto bob_msg = send_message(store, bob->id, *created.conversation_id, "not ann");
                assert(bob_msg.error == MessengerError::None && bob_msg.message_id);
                store.add_pain_suggestion("ann", {"wish", "not my message"}, "gmsg:" + std::to_string(*bob_msg.message_id));
                assert(store.pains_of("ann").size() == 4);
                auto gone = store.get_message(*sent_g.message_id);
                assert(gone);
                gone->deletion = DeletionState::DeletedForEveryone;
                store.update_message(*gone);
                assert(!store.pain_by_id(gmid));
                assert(store.pain_by_id(fpid) && store.pain_by_id(gpid) && store.pain_by_id(mpid));
            }
            {
                FileStore store(priv);
                auto rows = store.pains_of("ann");
                assert(rows.size() == 3);
                assert(store.pain_by_id(fpid) && store.pain_by_id(fpid)->source.rfind("fpost:", 0) == 0);
                assert(store.pain_by_id(gpid) && store.pain_by_id(gpid)->source.rfind("gpost:", 0) == 0);
                assert(store.pain_by_id(mpid) && store.pain_by_id(mpid)->source.rfind("mail:", 0) == 0);
                assert(!store.pain_by_id(gmid));
            }
            unlink(priv.c_str());
            unlink((priv + ".tmp").c_str());
            std::cout << "pain points: private sources survive flush\n";
        }

        // Old suggestions expire after 30 days; kept ones stay. Old 7-field mapp loads with pains on.
        const std::string hand = tmp_path();
        {
            FileStore store(hand);
            assert(create_account(store, breach, "cat", "correct-horse-battery-staple", "+15558880003").error == AuthError::None);
            store.add_post("cat", "x", "");
        }
        {
            std::string disk = slurp(hand);
            const std::int64_t now_s = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const std::string old_ts = std::to_string(now_s - 40LL * 24 * 3600);
            std::ofstream out(hand, std::ios::app);
            out << "pain\t7\tcat\twish\t0\t" << old_ts << "\tpost:1\told%20suggestion\n"
                << "pain\t8\tcat\twish\t1\t" << old_ts << "\tpost:1\told kept\n"
                << "mapp\tcat\t1\t1\t1\t1\t0\n";
        }
        {
            FileStore store(hand);
            assert(store.map_prefs_of("cat").pains); // 7-field row: default on
            auto c = store.pains_of("cat");
            assert(c.size() == 1 || c.size() == 2); // loading may or may not save at once...
            store.add_post("cat", "y", "");          // ...but the next save drops the expired suggestion
            c = store.pains_of("cat");
            assert(c.size() == 1 && c[0].phrase == "old kept" && !c[0].flowed); // 8 fields: Still
            assert(store.add_pain_suggestion("cat", {"wish", "new one here"}, "post:1") > 8); // ids continue past 8
        }
        unlink(hand.c_str());
        unlink((hand + ".tmp").c_str());

        // Flow is owner-only, kept-only, and the 9th field exists only while it is on.
        {
            const std::string fpath = tmp_path();
            std::uint64_t id = 0, sugg = 0, post_id = 0;
            {
                FileStore store(fpath);
                assert(create_account(store, breach, "ann", "correct-horse-battery-staple", "+15558880011").error == AuthError::None);
                assert(create_account(store, breach, "bob", "correct-horse-battery-staple", "+15558880012").error == AuthError::None);
                post_id = store.add_post("ann", "I wish the buses ran later", "");
                const std::string src = "post:" + std::to_string(post_id);
                id = store.add_pain_suggestion("ann", {"wish", "the buses ran later"}, src);
                sugg = store.add_pain_suggestion("ann", {"pain", "parking downtown"}, src);
                assert(!store.flow_pain("ann", id, true));          // still a suggestion
                assert(!store.pain_by_id(id)->flowed);
                assert(store.keep_pain("ann", id));
                assert(!store.flow_pain("bob", id, true));          // not bob's
                assert(!store.flow_pain("ann", sugg, true));        // unkept
                assert(!store.flow_pain("ann", 999, true));
                assert(store.flow_pain("ann", id, true));
                assert(store.flow_pain("ann", id, true));           // again: still true, no extra row
                assert(store.pain_by_id(id)->flowed && store.pain_by_id(id)->kept);
                assert(!store.pain_by_id(sugg)->flowed);
            }
            {
                const std::string disk = slurp(fpath);
                assert(disk.find("\tpost:" + std::to_string(post_id) + "\tthe buses ran later\t1\n") != std::string::npos);
                assert(disk.find("\tparking downtown\n") != std::string::npos);
                assert(disk.find("\tparking downtown\t") == std::string::npos); // suggestion stays 8 fields
                FileStore store(fpath);
                assert(store.pain_by_id(id)->flowed);
                assert(store.flow_pain("ann", id, false));
                assert(store.pain_by_id(id)->kept && !store.pain_by_id(id)->flowed);
            }
            {
                const std::string disk = slurp(fpath);
                assert(disk.find("\tthe buses ran later\n") != std::string::npos);
                assert(disk.find("\tthe buses ran later\t1\n") == std::string::npos); // Still drops the 9th field
                FileStore store(fpath);
                assert(!store.pain_by_id(id)->flowed);
                assert(store.flow_pain("ann", id, true) && store.forget_pain("ann", id));
                assert(!store.pain_by_id(id));
            }
            unlink(fpath.c_str());
            unlink((fpath + ".tmp").c_str());
        }
        // Same subject is the phrase, case-folded. A shared word is not a match.
        // Still and unkept rows are not in the Flowed set.
        {
            assert(pain_subject_key("  The   Buses ") == "the buses");
            assert(pain_subject_key("the buses") != pain_subject_key("more buses"));
            assert(pain_subject_key(" \t ") == "");
            const std::string spath = tmp_path();
            std::uint64_t ann_id = 0, bob_id = 0, cat_id = 0, dee_id = 0, sugg_id = 0;
            {
                FileStore store(spath);
                assert(create_account(store, breach, "ann", "correct-horse-battery-staple", "+15558880021").error == AuthError::None);
                assert(create_account(store, breach, "bob", "correct-horse-battery-staple", "+15558880022").error == AuthError::None);
                assert(create_account(store, breach, "cat", "correct-horse-battery-staple", "+15558880023").error == AuthError::None);
                assert(create_account(store, breach, "dee", "correct-horse-battery-staple", "+15558880024").error == AuthError::None);
                const auto ap = store.add_post("ann", "I wish the buses ran later", "");
                const auto ap2 = store.add_post("ann", "I wish the buses ran later too", "");
                const auto bp = store.add_post("bob", "I wish The Buses Ran Later", "");
                const auto cp = store.add_post("cat", "I wish the buses ran later", "");
                const auto dp = store.add_post("dee", "I wish more buses downtown", "");
                ann_id = store.add_pain_suggestion("ann", {"wish", "the buses ran later"}, "post:" + std::to_string(ap));
                sugg_id = store.add_pain_suggestion("ann", {"wish", "the buses ran later"}, "post:" + std::to_string(ap2));
                bob_id = store.add_pain_suggestion("bob", {"wish", "The Buses Ran Later"}, "post:" + std::to_string(bp));
                cat_id = store.add_pain_suggestion("cat", {"wish", "the buses ran later"}, "post:" + std::to_string(cp));
                dee_id = store.add_pain_suggestion("dee", {"wish", "more buses downtown"}, "post:" + std::to_string(dp));
                assert(ann_id && sugg_id && bob_id && cat_id && dee_id);
                assert(store.keep_pain("ann", ann_id) && store.flow_pain("ann", ann_id, true));
                assert(store.keep_pain("bob", bob_id) && store.flow_pain("bob", bob_id, true));
                assert(store.keep_pain("cat", cat_id));
                assert(!store.pain_by_id(cat_id)->flowed);
                assert(store.keep_pain("dee", dee_id) && store.flow_pain("dee", dee_id, true));
                assert(!store.pain_by_id(sugg_id)->kept);
                const auto same = store.flowed_with_subject("THE   buses   RAN later");
                assert(same.size() == 2);
                assert((same[0].id == ann_id && same[1].id == bob_id) || (same[0].id == bob_id && same[1].id == ann_id));
                const auto other = store.flowed_with_subject("more buses downtown");
                assert(other.size() == 1 && other[0].id == dee_id);
                assert(store.flowed_with_subject("buses").empty());
                assert(store.flowed_with_subject("").empty());
            }
            unlink(spath.c_str());
            unlink((spath + ".tmp").c_str());
        }
        // A hand-written 9th field reloads. Unkept + flowed is forced Still. 0 is Still.
        {
            const std::string rawp = tmp_path();
            {
                FileStore store(rawp);
                assert(create_account(store, breach, "dee", "correct-horse-battery-staple", "+15558880013").error == AuthError::None);
                store.add_post("dee", "x", "");
            }
            {
                std::ofstream out(rawp, std::ios::app);
                out << "pain\t3\tdee\twish\t1\t5\tpost:1\tflowed%20one\t1\n"
                    << "pain\t4\tdee\twish\t1\t5\tpost:1\tstill%20one\t0\n"
                    << "pain\t5\tdee\twish\t0\t5\tpost:1\tsuggestion%20one\t1\n";
            }
            {
                FileStore store(rawp);
                assert(store.pain_by_id(3)->flowed && store.pain_by_id(3)->phrase == "flowed one");
                assert(store.pain_by_id(4)->kept && !store.pain_by_id(4)->flowed);
                assert(store.pain_by_id(5) && !store.pain_by_id(5)->kept && !store.pain_by_id(5)->flowed);
            }
            unlink(rawp.c_str());
            unlink((rawp + ".tmp").c_str());
        }
        for (const char* badline : {"LGP1\npain\t1\tcat\twish\t1\t5\tpost:1\n", "LGP1\npain\t1\tcat\twish\t2\t5\tpost:1\tx\n",
                                    "LGP1\npain\t1\tcat\twish\t1\t5\tpost:1\tx\t2\n",
                                    // A 10th field is no longer malformed (2026-11-07): a newer version's
                                    // field is kept verbatim; see "forward compatibility" below.
                                    "LGP1\npainn\n"}) {
            const std::string bad = tmp_path();
            { std::ofstream out(bad); out << badline; }
            bool threw = false;
            try { FileStore store(bad); } catch (const std::runtime_error& e) {
                const std::string w = e.what();
                threw = w.find("bad pain") != std::string::npos || w.find("bad painn") != std::string::npos;
            }
            assert(threw);
            unlink(bad.c_str());
        }
        std::cout << "pain points: store ok (keep, forget, owner-only, off deletes, source/account deletion, expiry, reload, subject match)\n";
    }

    // Forward compatibility (2026-11-07): a NEWER version's extra fields on mapp and
    // pain rows load, survive saves (including a settings change) and are written back
    // verbatim, so rolling back one version neither refuses the store nor drops them.
    {
        const std::string fpath = tmp_path();
        {
            FileStore store(fpath);
            assert(create_account(store, breach, "fwd", "correct-horse-battery-staple", "+15559990001").error == AuthError::None);
            store.add_post("fwd", "source", "");
        }
        {
            std::ofstream out(fpath, std::ios::app);
            out << "mapp\tfwd\t1\t1\t1\t1\t0\t1\t0\tfuture%20x\n"
                << "pain\t40\tfwd\twish\t1\t5\tpost:1\tthe buses\t1\t7\tlater\n";
        }
        {
            FileStore store(fpath);
            assert(store.map_prefs_of("fwd").show && store.map_prefs_of("fwd").pains);
            auto p = store.pain_by_id(40);
            assert(p && p->kept && p->flowed && p->extra_fields.size() == 2);
            MapPrefs mp; mp.show = true; mp.friends = true; // a settings save knows only today's fields
            store.set_map_prefs("fwd", mp);
            assert(store.map_prefs_of("fwd").extra_fields.size() == 2);
        }
        {
            const std::string disk = slurp(fpath);
            assert(disk.find("mapp\tfwd\t1\t1\t1\t1\t1\t1\t0\tfuture%20x\n") != std::string::npos);
            assert(disk.find("\tthe buses\t1\t7\tlater\n") != std::string::npos);
            std::cout << "forward compatibility: newer mapp/pain fields kept through load, settings save and flush\n";
        }
        unlink(fpath.c_str());
        unlink((fpath + ".tmp").c_str());
    }

    unlink(path.c_str());
    unlink((path + ".tmp").c_str());
    std::cout << "\nall file_store tests passed\n";
    return 0;
}
