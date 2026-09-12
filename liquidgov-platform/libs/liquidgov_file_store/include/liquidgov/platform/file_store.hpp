#pragma once

// Durable AccountStore for the website process. Not the governance
// kernel ledger — PlatformAccount is not CitizenId, and this file is a
// mutable snapshot, not an append-only CivicAct log.
//
// Format (LGP1): UTF-8 text. First line is the magic "LGP1". Each later
// line is a record: type-tag, tab, percent-encoded fields. %  TAB  LF  CR
// are percent-encoded; everything else is literal. Atomic replace: write
// path+".tmp" (mode 0600), fsync, rename over path. Missing file = empty
// store. Non-empty file with a bad magic, unknown version, or malformed
// record is fail-closed (constructor throws; we do not boot empty and
// wipe users).
//
// Records:
//   n      <next_account_id>
//   a      <id> <username> <password_hash> <phone_verified 0|1> <created_at> <blurb>
//   s      <session_id> <account> <created_at> <last_seen> <expires_at> <revoked 0|1> <device_label>
//   f      <user_a> <user_b>          // one stored pair; lookup is undirected
//   r      <from_username> <to_username>
//   m      <from> <to> <text> [<ts>]  // ts optional (older files omit it)
//   ma     <from> <to> <text> <ts> <blob_id> <mime>   // mail with a file_transfer-ingested
//          attachment; bytes live in the blob dir, same store as avatars
//   ph     <account_id> <phone_fp>    // blake2b of signup phone; not the number
//   pq     <phone_fp> <until>         // 90-day quarantine after delete
//   post   <id> <author> <ts> <text> <community_slug-or-empty>
//   pn     <next_post_id>
//   reply  <id> <post_id> <author> <ts> <text>
//   repln  <next_reply_id>
//   sec    <post_id> <username>   // one line per (post, account) that has
//          seconded it; toggling off just means the line stops being written
//   prop   <id> <author> <ts> <title> <body>   // unofficial site-wide straw poll,
//          see Proposal's comment -- not a CitizenId vote
//   propn  <next_proposal_id>
//   pvote  <proposal_id> <voter> <choice 0-2>   // one line per (proposal, voter);
//          re-voting overwrites the prior line for that pair on the next flush
//   comm   <slug> <name> <owner> <about> <created_at>
//   cm     <slug> <username>
//   blk    <blocker> <blocked> [<at> <reason>]  // trailing two fields optional -- an
//          older 3-token line means an unknown time (0) and no reason
//   inner  <owner> <friend>
//   pref   <username> <findability 0-2> <friends_only_dm> <group_invite_confirm>
//          <presence_hide> <n_friend> <n_message> <n_group>
//          [<jukebox_policy 0-2> <profile_visibility 0-3> <posts_visibility 0-3>]
//          -- trailing three fields optional; an older 9-token line means all three
//          are their defaults (0/Public). A since-superseded 14-token shape (this
//          release's own earlier dev build: jukebox_policy plus four now-removed
//          hide_profile/posts_from_friends/others bools) is still accepted and
//          translated to the closest Visibility on load -- see the loader.
//   note   <id> <to> <type> <from> <ts> <read 0|1> <extra>
//   nn     <next_note_id>
//   av     <account_id> <blob_id> <created_at>   // self-portrait; bytes live in the blob dir
//   jt     <owner_username> <video_id> <added_at> [<queued_by>]  // jukebox queue entry;
//          video_id is an 11-char YouTube id already validated by extract_youtube_id, never
//          a raw URL. queued_by is who requested it -- absent (older lines) means the owner.
//   jh     <owner_username> <video_id> <queued_by> <played_at>  // one past jukebox play,
//          most-recent last; trimmed to kJukeboxHistoryCap on write
//   jsub   <subscriber_username> <owner_username>  // subscriber has added owner's personal
//          jukebox as a channel in their widget -- existence is the only state (no per-line
//          metadata), same shape as an f (friend) pair but directed, not mutual
//   gcn    <next_conversation_id>
//   gmsn   <next_message_id>
//   gc     <id> <is_group 0|1> <created_at>          // messenger.hpp Conversation, no roster
//   gcm    <conv_id> <account_id> <state 0=Active|1=Pending>  // one line per member, in
//          roster order; the gc line for conv_id is always written before its gcm lines
//   gmsg   <id> <conv_id> <sender> <sent_at> <edited_at> <deletion 0|1|2> <deleted_at>
//          <under_review 0|1> <text>                 // messenger.hpp Message, no edit_history
//   gmh    <message_id> <text>                       // one prior version, oldest first; the
//          gmsg line for message_id is always written before its gmh lines
//   ac     <account_id> <css>   // author CSS for /u/<username>/theme, already sanitized
//          (see sanitize_author_css in http_server.cpp) before it ever reaches here
//   cvf    <account_id> <frame_index> <blob_id>   // one canvas snapshot, in draw order;
//          bytes live in the blob dir, same as avatars. A fresh drawing clears all of an
//          account's cvf lines first (see clear_canvas_frames), so index is always 0..N-1
//          contiguous for whatever's currently stored.
//   cvm    <account_id> <timelapse 0|1>   // profile background mode: 0 = last frame as a
//          static image, 1 = loop through every frame. Absence = static (0).
//
// password_hash is argon2id from hash_password(). Never plaintext.
// Phone number is not stored (signup binding only). Sessions are secrets;
// the file is 0600.

#include "liquidgov/platform/auth.hpp"
#include "liquidgov/platform/avatar.hpp"
#include "liquidgov/platform/messenger.hpp"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace liquidgov::platform {

inline constexpr int kInnerCircleCap = 12;
inline constexpr std::int64_t kPhoneQuarantineSeconds = 90 * 24 * 60 * 60;

enum class Findability : int {
    FriendsOnly = 0,
    ExactMatch = 1,
    Typeahead = 2,
};

struct MailMessage {
    std::string from;
    std::string to;
    std::string text;
    std::int64_t ts = 0;
    // Set only for messages carrying a file_transfer-ingested attachment.
    // blob_id/mime come straight out of FileTransferResult; the original
    // filename is deliberately not kept (avoids header-injection surface
    // and lets the store name the download itself from mime).
    std::optional<std::string> attachment_blob_id;
    std::optional<std::string> attachment_mime;
};

struct Post {
    std::uint64_t id = 0;
    std::string author;
    std::int64_t ts = 0;
    std::string text;
    std::string community; // empty = personal feed
    // A repost is its own Post row (its own id/author/ts, in the reposting
    // user's own feed) that additionally names the ORIGINAL author/id --
    // not the post it was reposted from, if that itself was a repost, so
    // reposting a repost always credits the true original, same as
    // resharing an already-shared post.
    std::string repost_of_author; // empty = not a repost
    std::uint64_t repost_of_id = 0;
    // Optional single image/video attachment, already normalized/ingested
    // through liquidgov_file_transfer and living in the shared blob store
    // by the time it lands here -- this struct just carries the id/mime,
    // the same shape as Message's attachment fields.
    std::string attachment_blob_id; // empty = no attachment
    std::string attachment_mime;
};

struct PostReply {
    std::uint64_t id = 0;
    std::uint64_t post_id = 0;
    std::string author;
    std::int64_t ts = 0;
    std::string text;
};

struct Community {
    std::string slug;
    std::string name;
    std::string owner;
    std::string about;
    std::int64_t created_at = 0;
};

// A site-wide, unofficial straw poll -- explicitly not a binding vote (the
// genesis-phase copy is clear real governmental authority is a later,
// separate landslide). Any member can open one and any member can cast one
// vote on it, changeable, to gauge sentiment on something not yet decided.
struct Proposal {
    std::uint64_t id = 0;
    std::string author;
    std::int64_t ts = 0;
    std::string title;
    std::string body;
};

enum class ProposalVote : int {
    Yes = 0,
    No = 1,
    Abstain = 2,
};

struct ProposalTally {
    int yes = 0;
    int no = 0;
    int abstain = 0;
};

// Who besides the owner may add tracks to an account's jukebox. The owner can
// always curate their own queue (subject only to kJukeboxQueueCap) regardless
// of this setting.
enum class JukeboxPolicy : int {
    OwnerOnly = 0,
    Friends = 1,
    Anyone = 2,
};

// A minimum-trust ladder, most to least open: anyone at all, then anyone
// signed in, then friends only, then the owner alone. Each level implies
// every level after it can also see it -- there's no way to express "show
// strangers but hide friends" here (see Visibility below for that).
enum class Visibility : int {
    Public = 0,
    Members = 1,
    Friends = 2,
    OnlyMe = 3,
};

// Which slice of activity /feed shows -- see feed_view's comment for what
// each one actually pulls in.
enum class FeedFilter : int {
    All = 0,
    Friends = 1,
    Committees = 2,
    Drawings = 3,
};

struct AccountPrefs {
    Findability findability = Findability::ExactMatch;
    bool friends_only_dm = true;
    bool group_invite_confirm = true;
    bool presence_hide = false;
    bool notify_friend = true;
    bool notify_message = true;
    bool notify_group = true;
    JukeboxPolicy jukebox_policy = JukeboxPolicy::OwnerOnly;
    Visibility profile_visibility = Visibility::Public;
    Visibility posts_visibility = Visibility::Public;
};

// One block, with the context a settings page actually wants to show back
// -- when, and why (freeform, actor's own words, never shown to the
// blocked party). is_blocked/blocked_by stay the cheap boolean/list lookup
// they always were; this is the richer read for rendering the block list.
struct BlockEntry {
    std::string actor;
    std::string other;
    std::int64_t at = 0;
    std::string reason;
};

struct Notice {
    std::uint64_t id = 0;
    std::string to;
    std::string type; // friend_request | friend_accept | message | group_invite
    std::string from;
    std::int64_t ts = 0;
    bool read = false;
    std::string extra;
};

// Page-local jukebox queue entry for a profile (design: LIQUIDGOV_CONTEXT_HANDOFF.md
// "Jukebox", navigation option A — MPA, no site-wide SPA, stop on navigate away).
// The owner always curates their own queue; depending on the owner's
// JukeboxPolicy, friends or anyone else may also queue a track (see
// queued_by below). Only the owner can remove. No WebSocket sync — that is
// the in-world jukebox, a separate, unbuilt slice.
struct JukeboxTrack {
    std::string owner;      // username
    std::string video_id;   // validated 11-char YouTube id, never a raw URL
    std::int64_t added_at = 0;
    // Who requested this track. Equal to owner for the owner's own curated
    // additions, which stay queued after playing (today's behavior,
    // unaffected). Anything else is a guest request: one shot, removed from
    // the queue and logged to history the moment it finishes playing (see
    // complete_jukebox_track), and limited to one outstanding request per
    // guest at a time (enforced by the caller, same as policy/block checks).
    std::string queued_by;
};

// One past play, kept so a profile can show "things played in the past"
// even after a guest-requested track is gone from the live queue.
struct JukeboxHistoryEntry {
    std::string owner;
    std::string video_id;
    std::string queued_by;
    std::int64_t played_at = 0;
};

inline constexpr std::size_t kJukeboxQueueCap = 50;
inline constexpr std::size_t kJukeboxHistoryCap = 30;

bool valid_username(const std::string& username);
std::string fingerprint_phone(const std::string& e164);

// Extracts an 11-char YouTube video id from a small allow-listed set of URL
// shapes (youtube.com/watch?v=, youtu.be/<id>, youtube.com/embed/<id>,
// youtube-nocookie.com/embed/<id>). Returns nullopt for anything else. This
// is the only thing ever stored or rendered from a submitted URL — never the
// URL itself, never author HTML; the embed src is built by the platform from
// this id alone.
std::optional<std::string> extract_youtube_id(const std::string& url);

class FileStore : public AccountStore, public AvatarStore, public ConversationStore {
public:
    explicit FileStore(std::string path);
    ~FileStore() override = default;

    FileStore(const FileStore&) = delete;
    FileStore& operator=(const FileStore&) = delete;

    bool username_exists(const std::string& username) const override;
    std::size_t account_count() const;
    std::optional<PlatformAccount> find_by_username(const std::string& username) const override;
    AccountId create(const PlatformAccount& account) override;
    void save_session(const Session& session) override;
    std::vector<Session> sessions_for(AccountId account) const override;
    void revoke_session(const SessionId& id) override;

    std::optional<PlatformAccount> find_by_id(AccountId id) const;
    std::optional<Session> get_session(const SessionId& id) const;
    void touch_session(const SessionId& id);

    void set_blurb(AccountId id, const std::string& blurb);
    std::string blurb(AccountId id) const;

    // Sanitized (see sanitize_author_css in http_server.cpp) CSS for the
    // account's own /u/<username>/theme document — never the site's own
    // pages. Storage layer trusts the caller already sanitized; it just
    // persists the value.
    void set_author_css(AccountId id, const std::string& css);
    std::string author_css(AccountId id) const;

    // Canvas: an ordered sequence of blob-stored PNG snapshots captured
    // while the account was drawing, used to set the profile theme's
    // background (see theme_background_css in http_server.cpp) — either
    // the last frame as a static image, or all of them as a CSS-only
    // looping animation. clear_canvas_frames wipes the whole sequence
    // (called before a fresh drawing starts); add_canvas_frame appends the
    // next one.
    void clear_canvas_frames(AccountId id);
    void add_canvas_frame(AccountId id, const std::string& blob_id);
    std::vector<std::string> canvas_frames(AccountId id) const; // blob ids, draw order
    void set_canvas_timelapse(AccountId id, bool on);
    bool canvas_timelapse(AccountId id) const;
    // For BlobStore::adopt() at process startup -- see main()'s comment.
    std::vector<std::string> all_canvas_frame_blob_ids() const;

    bool set_password_hash(AccountId id, const std::string& password_hash);

    bool are_friends(const std::string& a, const std::string& b) const;
    std::vector<std::string> friends_of(const std::string& user) const;
    void add_friend_request(const std::string& from, const std::string& to);
    bool accept_friend_request(const std::string& from, const std::string& to);
    void unfriend(const std::string& actor, const std::string& other);
    std::vector<std::string> incoming_requests(const std::string& user) const;
    std::vector<std::string> outgoing_requests(const std::string& user) const;

    void add_mail(const std::string& from, const std::string& to, const std::string& text,
                  const std::optional<std::string>& attachment_blob_id = std::nullopt,
                  const std::optional<std::string>& attachment_mime = std::nullopt);
    std::vector<MailMessage> mail_with(const std::string& a, const std::string& b) const;
    std::vector<MailMessage> mail_sent_by(const std::string& from) const;

    // Authorization lookup for attachment download: returns the message
    // only when `user` is sender or recipient of it. Caller still applies
    // the current friends/not-blocked gate the same way the mail thread
    // view does — a message with an attachment does not outlive that gate.
    std::optional<MailMessage> find_mail_attachment(const std::string& user,
                                                     const std::string& blob_id) const;
    // For startup blob-store adoption, same reason avatars are adopted:
    // FilesystemBlobStore's refcount metadata is in-memory only.
    std::vector<MailMessage> all_mail_with_attachments() const;

    void block(const std::string& actor, const std::string& other, const std::string& reason = "");
    bool is_blocked(const std::string& blocker, const std::string& blocked) const;
    std::vector<std::string> blocked_by(const std::string& actor) const;
    std::vector<BlockEntry> block_entries_of(const std::string& actor) const;
    void unblock(const std::string& actor, const std::string& other);

    bool add_inner(const std::string& owner, const std::string& friend_name);
    void remove_inner(const std::string& owner, const std::string& friend_name);
    std::vector<std::string> inner_of(const std::string& owner) const;

    AccountPrefs prefs_of(const std::string& username) const;
    void set_prefs(const std::string& username, const AccountPrefs& p);

    bool is_findable_exact(const std::string& requester, const std::string& target) const;
    std::vector<std::string> typeahead(const std::string& requester, const std::string& prefix) const;

    std::uint64_t add_post(const std::string& author, const std::string& text, const std::string& community,
                           const std::string& repost_of_author = "", std::uint64_t repost_of_id = 0,
                           const std::string& attachment_blob_id = "", const std::string& attachment_mime = "");
    std::vector<Post> feed_for(const std::string& user, std::size_t limit) const;
    // Friends is exactly feed_for. All and Committees additionally pull in
    // posts from communities `user` actually belongs to (feed_for never
    // includes community posts at all) -- "committees," not every
    // community on the site, same personal-feed posture feed_for already
    // has. Drawings is whichever of those carries a media attachment;
    // there's no separate "hand-drawn" flag anywhere in Post, so this is
    // the closest honest reading of "drawings" the data model supports.
    std::vector<Post> feed_view(const std::string& user, std::size_t limit, FeedFilter filter) const;
    std::vector<Post> posts_in(const std::string& community, std::size_t limit) const;
    std::vector<Post> posts_by(const std::string& author) const;
    std::optional<Post> post_by_id(std::uint64_t id) const;
    // For BlobStore::adopt() at process startup -- see main()'s comment.
    std::vector<Post> all_posts_with_attachments() const;

    std::uint64_t add_reply(std::uint64_t post_id, const std::string& author, const std::string& text);
    std::vector<PostReply> replies_of(std::uint64_t post_id) const; // oldest first

    // A lightweight "I support this" mark, separate from a reply -- one
    // per (post, account), toggled on/off, never a running log the way
    // proposal votes are (there's no "second" choice to change between,
    // just on or off).
    void toggle_second(std::uint64_t post_id, const std::string& user);
    bool has_seconded(std::uint64_t post_id, const std::string& user) const;
    int second_count(std::uint64_t post_id) const;

    std::uint64_t add_proposal(const std::string& author, const std::string& title, const std::string& body);
    std::vector<Proposal> all_proposals() const; // oldest first, same as posts_by
    std::optional<Proposal> proposal_by_id(std::uint64_t id) const;
    // Casting again overwrites the voter's own prior choice on this
    // proposal -- one live vote per (proposal, voter), not a running log.
    void cast_proposal_vote(std::uint64_t proposal_id, const std::string& voter, ProposalVote choice);
    std::optional<ProposalVote> proposal_vote_of(std::uint64_t proposal_id, const std::string& voter) const;
    ProposalTally tally_of(std::uint64_t proposal_id) const;

    bool create_community(const Community& c);
    std::optional<Community> community(const std::string& slug) const;
    std::vector<Community> all_communities() const;
    bool is_member(const std::string& slug, const std::string& username) const;
    bool join_community(const std::string& slug, const std::string& username);
    bool invite_community(const std::string& slug, const std::string& actor, const std::string& target);
    void leave_community(const std::string& slug, const std::string& username);
    bool delete_community(const std::string& slug, const std::string& actor);
    std::vector<std::string> members_of(const std::string& slug) const;
    std::vector<Community> communities_of(const std::string& username) const;

    void add_notice(const std::string& to, const std::string& type, const std::string& from,
                    const std::string& extra);
    std::vector<Notice> notices_for(const std::string& user, std::size_t limit) const;
    int unread_count(const std::string& user) const;
    void mark_notices_read(const std::string& user);

    void set_phone_fp(AccountId id, const std::string& fp);
    bool phone_quarantined(const std::string& fp) const;
    bool delete_account(AccountId id);

    std::optional<Avatar> get(AccountId owner) const override;
    void set(const Avatar& avatar) override;
    void erase(AccountId owner) override;
    std::vector<Avatar> all_avatars() const;

    // ConversationStore — messenger groups. See messenger.hpp for the
    // business rules; this is storage only, same relationship FileStore
    // has to auth.hpp/avatar.hpp above.
    ConversationId create_conversation(const Conversation& conv) override;
    std::optional<Conversation> get_conversation(ConversationId id) const override;
    void save_conversation(const Conversation& conv) override;
    std::vector<Conversation> conversations_for(AccountId account) const override;
    MessageId save_message(const Message& msg) override;
    std::optional<Message> get_message(MessageId id) const override;
    void update_message(const Message& msg) override;
    std::vector<Message> all_messages() const override;
    // Convenience for rendering a thread: all messages in a conversation,
    // oldest first. Not part of ConversationStore — messenger.hpp never
    // needs "just this conversation's messages" itself.
    std::vector<Message> messages_in(ConversationId conv_id) const;

    // Owner must be an existing account; video_id must already be a validated
    // id (see extract_youtube_id). queued_by is who's requesting it -- pass
    // owner itself for the owner's own curated additions. Returns false on
    // unknown owner, invalid id, or a full queue (kJukeboxQueueCap) — caller
    // renders the refusal. Policy (who besides the owner may add), block,
    // and one-outstanding-request-per-guest checks are the caller's job
    // (http_server.cpp), same division extract_youtube_id validation already is.
    bool add_jukebox_track(const std::string& owner, const std::string& video_id,
                           const std::string& queued_by);
    // Removes the first matching (owner, video_id) entry, if any. No-op if
    // absent — same shape as remove_inner. Does not touch history.
    void remove_jukebox_track(const std::string& owner, const std::string& video_id);
    std::vector<JukeboxTrack> jukebox_of(const std::string& owner) const;
    // Called when a track finishes playing. Always appends a history entry,
    // using the track's own stored queued_by (never a caller-supplied one).
    // A guest-requested track (queued_by != owner) is also removed from the
    // queue -- one shot, then gone; an owner-curated track stays queued,
    // unaffected. No-op if the track isn't currently queued.
    void complete_jukebox_track(const std::string& owner, const std::string& video_id);
    std::vector<JukeboxHistoryEntry> jukebox_history_of(const std::string& owner) const;

    // A personal jukebox as a channel in the floating widget's station list
    // (see jukebox_script in http_server.cpp): subscribing is what makes the
    // widget's local "add a station" list mean something server-side, since
    // otherwise it's just this device's own localStorage. Returns false on
    // no-op (already in that state, or subscriber == owner). Listening
    // itself was already open to anyone who can view the profile -- this
    // only adds "the owner can see who's tuned in" and a durable list this
    // account can find itself again from another device.
    bool subscribe_jukebox(const std::string& subscriber, const std::string& owner);
    bool unsubscribe_jukebox(const std::string& subscriber, const std::string& owner);
    bool is_subscribed_jukebox(const std::string& subscriber, const std::string& owner) const;
    std::vector<std::string> jukebox_subscriptions_of(const std::string& subscriber) const;
    std::size_t jukebox_subscriber_count(const std::string& owner) const;

    const std::string& path() const { return path_; }

private:
    void load();
    void flush();
    void drop_requests_between(const std::string& a, const std::string& b);
    void strip_inner_both(const std::string& a, const std::string& b);

    std::string path_;
    AccountId next_id_ = 1;
    std::uint64_t next_post_id_ = 1;
    std::uint64_t next_note_id_ = 1;
    std::map<std::string, AccountId> by_username_;
    std::map<AccountId, PlatformAccount> accounts_;
    std::map<SessionId, Session> sessions_;
    std::map<AccountId, std::string> blurbs_;
    std::map<AccountId, std::string> author_css_;
    std::map<AccountId, std::vector<std::string>> canvas_frames_;
    std::map<AccountId, bool> canvas_timelapse_;
    std::map<AccountId, std::string> phone_fp_;
    std::map<std::string, std::int64_t> quarantine_;
    std::set<std::pair<std::string, std::string>> friends_;
    std::vector<std::pair<std::string, std::string>> requests_;
    std::vector<MailMessage> mail_;
    std::set<std::pair<std::string, std::string>> blocks_;
    std::map<std::pair<std::string, std::string>, BlockEntry> block_meta_;
    std::map<std::string, std::set<std::string>> inner_;
    std::map<std::string, AccountPrefs> prefs_;
    std::vector<Post> posts_;
    std::uint64_t next_reply_id_ = 1;
    std::vector<PostReply> replies_;
    std::set<std::pair<std::uint64_t, std::string>> seconds_;
    std::uint64_t next_proposal_id_ = 1;
    std::vector<Proposal> proposals_;
    std::map<std::uint64_t, std::map<std::string, ProposalVote>> proposal_votes_;
    std::map<std::string, Community> communities_;
    std::map<std::string, std::set<std::string>> members_;
    std::vector<Notice> notices_;
    std::map<AccountId, Avatar> avatars_;
    std::map<std::string, std::vector<JukeboxTrack>> jukebox_;
    std::map<std::string, std::vector<JukeboxHistoryEntry>> jukebox_history_;
    std::set<std::pair<std::string, std::string>> jukebox_subs_; // (subscriber, owner)
    ConversationId next_conv_id_ = 1;
    MessageId next_message_id_ = 1;
    std::map<ConversationId, Conversation> conversations_;
    std::map<MessageId, Message> messages_;
};

} // namespace liquidgov::platform
