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
//   ml     <username> <email>   // deliverable signup email, lowercased -- unlike ph/
//          Profile.email_fp (both one-way fingerprints), this is the reversible address
//          password-reset mail actually goes to. One row per username, overwritten on
//          re-set, never appended. Kept a true 1:1 index (one username per email): if an
//          address is already claimed by a *different* username, set_email() silently
//          no-ops rather than stealing the mapping -- that account's signup/settings
//          change still succeeds, it just doesn't get recovery/Google-linking wired to
//          that address until it uses a different one.
//   pr     <token_hash> <username> <until_unix>   // one outstanding password-reset
//          token. token_hash is sha256_hex of the raw 32-byte token mailed to the user
//          (the raw token itself is never written to disk); until_unix is a 1-hour
//          expiry. One row per username -- a new /forgot request overwrites any prior
//          unused one rather than appending, and a successful /reset consumes (erases)
//          it. An expired-but-never-used row is garbage-collected the next time
//          anything touches it (consume_reset_token), not swept proactively.
//   gid    <username> <google_sub>   // Google "sub" (subject) claim linked to this
//          account, from a verified Google Identity Services ID token -- how a Google
//          one-click login is matched back to an account (see username_by_google_sub).
//          One row per username; a username has at most one linked Google identity.
//   post   <id> <author> <ts> <text> <community_slug-or-empty>
//   pn     <next_post_id>
//   reply  <id> <post_id> <author> <ts> <text>
//   repln  <next_reply_id>
//   sec    <post_id> <username>   // one line per (post, account) that has
//          seconded it; toggling off just means the line stops being written
//   prop   <id> <author> <ts> <title> <body>   // unofficial site-wide straw poll,
//          see Proposal's comment -- not a CitizenId vote. Shape is frozen at
//          5 fields so existing files boot unchanged.
//   propn  <next_proposal_id>
//   pvote  <proposal_id> <voter> <choice 0-2>   // Yes/No/Abstain only
//   pbk    <proposal_id> <kind 1-4> <unit> <min> <max> <step>
//          ballot kind other than YesNo; absence means YesNo. Never reuse pvote.
//   pbo    <proposal_id> <index> <label>        // Choice/Scale labels
//   pbv    <proposal_id> <voter> <payload>      // number/date/choice/scale value
//          or a reserved word (Brief 7 item 4): "skip" on any non-YesNo kind, "none" (None of these) on a Choice.
//          Rollback: an older binary reads "skip" as a date/number; drop those rows first (REPORT7).
//   popen  <proposal_id>                        // Choice write-ins on. Absence = closed.
//          Separate tag: pbk is exact 7 fields (loader throws otherwise).
//   pba    <proposal_id> <index> <username>     // who added that pbo row. No row = creator-seeded.
//   poff   <proposal_id> <jurisdiction>         // Officials race. Absence = ordinary proposal.
//   pcity  <proposal_id> <city-key>             // optional city this proposal is about.
//          The key is city_key() of a city some group already names. Absence = not
//          tied to a city. Kept even while no group names that city (it shows
//          again once one does; Brandon 2026-11-06). Not poff.
//   pwho   <proposal_id> <text>                 // "who could act on it", free text.
//          Absence = blank. Not the officials jurisdiction.
//   ctag   <city-key> <tag>                     // a department or council members of
//          that city named. Cap 80, same tidy as a city name. A second tag that
//          differs only by letter case is not stored. Kept even while no group
//          names that city (Brandon 2026-11-06).
//   ptag   <proposal_id> <tag>                  // one of its city's tags this proposal
//          picks. A tag the city does not have, or a proposal with no city, is dropped.
//   prply  <id> <proposal_id> <author> <ts> <text>  // a comment, same shape as reply.
//          As public as the proposal. A comment whose proposal is gone is dropped.
//   prpln  <next_proposal_reply_id>
//   prx    <proposal_id> <username> <kind>     // one reaction per person. kind is
//          support, oppose, question, or noted. A later row for the same person
//          replaces the earlier one. Absence = none. A bad kind or a missing
//          proposal is dropped. Not a vote. Posts do not use this tag.
//   comm   <slug> <name> <owner> <about> <created_at> [<category 0-10> <subcategory>]
//          -- trailing two fields optional; an older 6-token line means category
//          General (10) and an empty subcategory
//   cm     <slug> <username>
//   ccity  <slug> <city>   // the city a group says it is in (Community.city). A
//          label, not an AVND locality: it confers no standing. Absence = no city.
//          Separate tag so comm keeps its frozen shape; a ccity whose comm is
//          missing is dropped on load.
//   pain   <id> <owner> <wish|pain> <kept 0|1> <ts> <source> <phrase> [<flowed 1>]
//          a wish or pain point noticed in the owner's own public words (PainPoint).
//          Private to the owner until they Flow a kept one. The 9th field is written
//          only when flowed is true, so a Still row stays 8 fields and an older
//          binary can still open it. A 9th field of 0 is accepted and then omitted.
//          Source is post:<id>, prop:<id> or prply:<id>; a row whose source is gone,
//          or an unkept one older than 30 days, is dropped.
//   painn  <next_pain_id>
//   mapp   <username> <show> <groups> <posts> <comments> <friends> [<pains>]  // each 0|1:
//          what the map may show about this person (MapPrefs). Absence = the
//          defaults (all on except friends). Separate tag so pref keeps its shape.
//   clist  <city-key> <username>  // this person chose to be listed on that
//          city's page. Absence = not listed. A row whose person is not a
//          member of a group in that city is dropped on load.
//   blk   <blocker> <blocked> [<at> <reason>]  // trailing two fields optional -- an
//          older 3-token line means an unknown time (0) and no reason
//   inner  <owner> <friend>
//   pref   <username> <findability 0-2> ...
//   prof   <username> <email_fp> <display_name> <hometown> <living> <interests> <career> <school>
//          optional; absence means empty profile. email_fp is blake2 of the
//          address (same helper as phone). display_name is plaintext.
//          <presence_hide> <n_friend> <n_message> <n_group>
//          [<jukebox_policy 0-2> <profile_visibility 0-3> <posts_visibility 0-3>
//          [<intro_every_visit 0|1>]]
//          -- trailing three fields optional; an older 9-token line means all three
//          are their defaults (0/Public). intro_every_visit is a further-trailing
//          fourth optional field; a 12-token line (without it) means its own
//          default (1, on, since 2026-11-13; prefs.hpp). A since-superseded 14-token shape (this release's own
//          earlier dev build: jukebox_policy plus four now-removed hide_profile/
//          posts_from_friends/others bools) is still accepted and translated to
//          the closest Visibility on load -- see the loader.
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
//   gc     <id> <is_group 0|1> <created_at> [<title>]
//          -- title is optional; an older 4-token line means empty title
//   gcm    <conv_id> <account_id> <state 0=Active|1=Pending>  // one line per member, in
//          roster order; the gc line for conv_id is always written before its gcm lines
//   gmsg   <id> <conv_id> <sender> <sent_at> <edited_at> <deletion 0|1|2> <deleted_at>
//          <under_review 0|1> <text> [<blob_id> <mime>]
//          -- trailing two fields optional; an older 10-token line means no attachment
//   gmh    <message_id> <text>                       // one prior version, oldest first; the
//          gmsg line for message_id is always written before its gmh lines
//   mlr    <username> <thread_key> <last_read_unix>
//          -- thread_key is "d:<other>" for 1:1 mail or "g:<conv_id>" for a group.
//          Absence means never opened. Used only for unread dots, not access control.
//   ac     <account_id> <css>   // author CSS for /u/<username>/theme, already sanitized
//          (see sanitize_author_css in http_server.cpp) before it ever reaches here
//   here   <username> <0|1> [...]   // whether this person shows as a dot on profile pages they
//          visit (Brandon 2026-11-12). A required choice at signup, nothing pre-selected; absence =
//          not chosen yet = hidden (asked once on a profile page). Positions are never stored
//          (memory only). Dropped on load when the person is gone; extra trailing fields accepted.
//          (Rollback: an older binary refuses it; strip it first with deploy/here-undo.py.)
//   sty    <id> <author> <created_at> <name> <css> [...]   // one published profile style
//          (ProfileStyle; Brandon 2026-11-12, profile CSS slice 2): a frozen copy of the author's
//          Profile CSS when they published it; never edited, only removed. The site checks the
//          CSS with sanitize_author_css at publish, use and render time. Dropped on load when
//          the author is gone. Extra trailing fields are accepted (forward compatible), as on
//          stu, str and styn, so a later release can add one without breaking a rollback.
//   styn   <next_style_id>
//   stu    <style_id> <username> <ts>   // this person copied that style into their own
//          Profile CSS (newest ts = last copy). One row per (style, person): "most used" counts
//          people, never the author. "Based on" is the person's newest use whose style still
//          exists. Dropped with the style, or when the person is gone.
//   str    <style_id> <reporter> <ts> <reason>   // one report per (style, reporter); a later
//          report replaces the reason. For /admin/styles. Dropped with the style or reporter.
//   (Rollback: an older binary refuses these four; strip them first with deploy/styles-undo.py.)
//   cvf    <account_id> <frame_index> <blob_id>   // one canvas snapshot, in draw order;
//          bytes live in the blob dir, same as avatars. A fresh drawing clears all of an
//          account's cvf lines first (see clear_canvas_frames), so index is always 0..N-1
//          contiguous for whatever's currently stored.
//   cvm    <account_id> <timelapse 0|1>   // profile background mode: 0 = last frame as a
//          static image, 1 = loop through every frame. Absence = static (0).
//   em     <account_id> <email_fp>   // leftover from an earlier, since-abandoned schema
//          that fingerprinted email separately from prof (now folded into prof's own
//          email_fp field) -- no longer written, but tolerated on load (silently
//          ignored) so a store carrying old lines from that schema still boots instead
//          of failing closed on a tag nothing here writes anymore.
//   jauto  <username> <0|1>   // "play my Juketube when someone opens my profile" --
//          absent means off (default). A separate tag rather than another pref field:
//          the pref line already grew one ambiguous trailing-optional shape too many
//          (see its own comment) -- a same-length extension there would have collided
//          with the already-claimed 14-token legacy meaning.
//   jrelay <username> <target>   // "play this Juketube instead" -- <username>'s own
//          queue/now-playing is relayed from <target>'s instead of playing its own.
//          Absent or empty target means no relay (play your own queue, the default).
//          Resolved at most one hop at read time (see jukebox_effective_owner) --
//          self-pointers, a missing target, and A->B->A cycles all fall back to the
//          account's own queue rather than being stored invalid or chased further.
//   jpos   <station> <video_id> <offset_sec> <updated_unix>   // the shared
//          playhead's freeze point: where a station's current track was when
//          its last listener left (or its most recent periodic save while
//          still playing). station is "" for the site-wide station, "c:<slug>"
//          for a community's, "u:<username>" for a personal jukebox. One row
//          per station, always overwritten, never appended. Absent means "no
//          freeze on record" -- a station with nobody in it doesn't keep
//          advancing elapsed time the way it briefly did before this tag
//          existed; playback for that track starts from 0 the next time
//          anyone shows up, same as if it had just been queued.
//   pau    <post_id> <0=Friends|1=Anybody|2=GroupsOnly>   // a post's audience,
//          the /feed "Anybody"/"Friends"/"Groups" chips. Absent means Friends,
//          matching every post written before this field existed. Friends and
//          GroupsOnly both need Post.community/pg to actually matter; a
//          "Friends + Groups" post is audience=Friends (0) WITH one or more
//          slugs set -- that combination, not a 4th value, is what puts a
//          community post in the author's own personal feed too. A separate
//          tag, not a wider "post" record, for the same reason "rp"/"pa"
//          already are -- existing 6-token "post" lines load unchanged.
//   px     <post_id> <username>   // Anybody hide-list: this username must not
//          see the post (feed or profile), even though Anybody would
//          otherwise show it to them. Meaningless when audience isn't
//          Anybody. One line per excepted username; absent entirely means no
//          exceptions (truly public-on-profile).
//   pf     <post_id> <username>   // Friends show-list: only these usernames
//          (plus the author) see the post in the personal/friends feed.
//          Meaningless when audience isn't Friends. Absent entirely means all
//          current friends (today's only behavior, unchanged for every post
//          before this field existed).
//   pg     <post_id> <slug>   // an ADDITIONAL community this post also
//          appears in, beyond Post.community itself (which still holds the
//          first/primary slug for back-compat with every reader that only
//          ever looked at that one field). One line per extra slug.
//   rp     <post_id> <origin_author> <origin_post_id>   // this post is a repost
//          (credits the original author). Separate tag so old "post" lines load unchanged.
//   pa     <post_id> <blob_id> <mime>   // the post's attachment. Same reasoning as rp.
//   pv     <post_id> <old Visibility 0|2>   // LEGACY, read only, never written:
//          converted to the pau audience on load (see the note below).
//
// Legacy note: an earlier same-day draft of per-post audience used a
// two-value "pv <post_id> <old Visibility 0|2>" tag (0=Public, 2=Friends)
// before Groups-exceptions/allowlists existed. "pv" is no longer written --
// tolerated on load and converted into the pau/audience model above
// (Public->Anybody, anything else->Friends) -- so the one post that used it
// before this replaced it still boots and renders correctly.
//   lk     <username>   // operator-panel lock: presence means this account
//          cannot log in (existing sessions revoked the moment it's set).
//          Absent means not locked, the default for every account. Login's
//          own error stays the generic "Invalid username or password" --
//          this tag is never surfaced to the person it's applied to.
//   alog   <ts> <operator> <action> <target>   // one operator-panel
//          action, newest last (same append order as everything else here);
//          trimmed to kAuditLogCap on write. action is one of lock/unlock/
//          logout_all/delete_account/delete_post/delete_community/
//          programs_grant/programs_revoke (and since 2026-11-08/12 delete_proposal,
//          delete_comment, delete_style, dismiss_style_reports); target is a bare username,
//          post/style id, or slug -- never a phone/email fingerprint or anything else
//          that counts as PII.
//   prgr   <username> <kinds> <max_bytes> <granted_by> <granted_at>   // an
//          operator's permission to send programs in mail (ProgramGrant):
//          kinds is the ProgramKind bitmask. Absent = no programs.
//   afn    <key> <name>   // download name of a program attachment, per
//          message (attachment_name_key: "d|from|to|ts|blob" for 1:1 mail,
//          "g|<message id>" for a group message), sanitized to
//          [A-Za-z0-9 ._()+-] before it is stored so it can go in a
//          Content-Disposition header (other attachments are still named
//          from their mime, see MailMessage). Pruned when the message or
//          an account in it is gone (at load and on account deletion).
//
// NOTE: the loader rejects unknown tags, so a binary older than prgr/afn
// cannot read a file written once a grant or program attachment exists.
// Roll back by restoring the pre-deploy .lgp copy, not just the binary.
//
// password_hash is argon2id from hash_password(). Never plaintext.
// Phone number is not stored (signup binding only). Sessions are secrets;
// the file is 0600.

#include "liquidgov/platform/auth.hpp"
#include "liquidgov/platform/avatar.hpp"
#include "liquidgov/platform/messenger.hpp"
#include "liquidgov/platform/records/social.hpp"
#include "liquidgov/platform/records/accounts.hpp"
#include "liquidgov/platform/records/mail.hpp"
#include "liquidgov/platform/records/admin.hpp"
#include "liquidgov/platform/records/prefs.hpp"
#include "liquidgov/platform/records/posts.hpp"
#include "liquidgov/platform/records/communities.hpp"
#include "liquidgov/platform/records/city.hpp"
#include "liquidgov/platform/records/pains.hpp"
#include "liquidgov/platform/records/proposals.hpp"
#include "liquidgov/platform/records/styles.hpp"
#include "liquidgov/platform/records/profile.hpp"
#include "liquidgov/platform/records/notices.hpp"
#include "liquidgov/platform/records/jukebox.hpp"

#include <iosfwd>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace liquidgov::platform {

// Each record family's types live in its own header (include/liquidgov/platform/records/<family>.hpp).
class FileStore : public AccountStore, public AvatarStore, public ConversationStore {
public:
    explicit FileStore(std::string path);
    ~FileStore() override = default;

    FileStore(const FileStore&) = delete;
    FileStore& operator=(const FileStore&) = delete;

    bool username_exists(const std::string& username) const override;
    std::size_t account_count() const;
    std::size_t post_count() const;
    std::optional<PlatformAccount> find_by_username(const std::string& username) const override;
    AccountId create(const PlatformAccount& account) override;
    void save_session(const Session& session) override;
    std::vector<Session> sessions_for(AccountId account) const override;
    void revoke_session(const SessionId& id) override;

    std::optional<PlatformAccount> find_by_id(AccountId id) const;
    // Operator panel only -- never renders password_hash to HTML at the
    // call site, but the struct still carries it, so callers must not
    // either. Newest account first, matching every other list on the site.
    std::vector<PlatformAccount> all_accounts() const;
    std::optional<Session> get_session(const SessionId& id) const;
    void touch_session(const SessionId& id);

    void set_blurb(AccountId id, const std::string& blurb);
    std::string blurb(AccountId id) const;

    void set_profile(const std::string& username, const Profile& p);
    Profile profile_of(const std::string& username) const;
    std::vector<std::string> findable_sample(const std::string& requester, std::size_t n) const;

    // Sanitized (see sanitize_author_css in http_server.cpp) CSS for the
    // account's own /u/<username>/theme document — never the site's own
    // pages. Storage layer trusts the caller already sanitized; it just
    // persists the value.
    void set_author_css(AccountId id, const std::string& css);
    std::string author_css(AccountId id) const;

    // Dots on profile pages ("here"): nullopt = not chosen yet (hidden). Ignored for an unknown username.
    std::optional<bool> here_choice(const std::string& username) const;
    void set_here_choice(const std::string& username, bool on);
    // The profile style library (sty/styn/stu/str). The store checks only that the author/person exists and
    // that name and CSS are non-empty; the site checks the CSS (sanitize_author_css) and the name's length.
    std::optional<std::uint64_t> publish_style(const std::string& author, const std::string& name,
                                               const std::string& css);
    std::vector<ProfileStyle> styles() const;                 // newest first
    std::optional<ProfileStyle> style(std::uint64_t id) const;
    bool remove_style(std::uint64_t id);                      // with its use and report rows
    bool use_style(std::uint64_t id, const std::string& username);  // records (or refreshes) one use
    std::size_t style_use_count(std::uint64_t id) const;      // people other than the author
    std::optional<ProfileStyle> style_based_on(const std::string& username) const;
    bool report_style(std::uint64_t id, const std::string& reporter, const std::string& reason);
    std::vector<StyleReport> style_reports(std::uint64_t id) const;  // oldest first
    bool dismiss_style_reports(std::uint64_t id);

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

    // Password recovery / Google sign-in -- see the ml/pr/gid tag comments
    // in the LGP1 registry above. email passed to set_email/username_by_email
    // is stored/compared lowercased by the caller (http_server.cpp), not
    // re-normalized here -- FileStore just keys on whatever string it's given.
    void set_email(const std::string& username, const std::string& email);
    std::optional<std::string> email_of(const std::string& username) const;
    std::optional<std::string> username_by_email(const std::string& email) const;

    void set_reset_token(const std::string& username, const std::string& token_hash,
                          std::int64_t until_unix);
    // Looks up token_hash across all outstanding reset tokens. A valid
    // (unexpired) match is consumed -- erased before returning, so a
    // token works exactly once -- and its owning username is returned.
    // A match that's found but already past until_unix is also erased
    // (garbage-collected on next touch) and treated as not found.
    std::optional<std::string> consume_reset_token(const std::string& token_hash);

    void set_google_id(const std::string& username, const std::string& google_sub);
    std::optional<std::string> username_by_google_sub(const std::string& google_sub) const;

    bool are_friends(const std::string& a, const std::string& b) const;
    std::vector<std::string> friends_of(const std::string& user) const;
    void add_friend_request(const std::string& from, const std::string& to);
    bool accept_friend_request(const std::string& from, const std::string& to);
    // The receiver says no: the request from -> to is dropped without a notice (2026-11-10, People page Decline).
    // False when there was no such request.
    bool decline_friend_request(const std::string& from, const std::string& to);
    void unfriend(const std::string& actor, const std::string& other);
    std::vector<std::string> incoming_requests(const std::string& user) const;
    std::vector<std::string> outgoing_requests(const std::string& user) const;

    void add_mail(const std::string& from, const std::string& to, const std::string& text,
                  const std::optional<std::string>& attachment_blob_id = std::nullopt,
                  const std::optional<std::string>& attachment_mime = std::nullopt);
    std::vector<MailMessage> mail_with(const std::string& a, const std::string& b) const;
    std::vector<MailMessage> mail_sent_by(const std::string& from) const;

    // People this account has exchanged 1:1 mail with (either direction).
    std::vector<std::string> mail_peers(const std::string& user) const;
    std::int64_t last_mail_ts(const std::string& a, const std::string& b) const;
    std::string last_mail_preview(const std::string& a, const std::string& b) const;

    // Unread dots. thread_key is "d:<username>" or "g:<conv_id>".
    void mark_thread_read(const std::string& user, const std::string& thread_key);
    std::int64_t thread_last_read(const std::string& user, const std::string& thread_key) const;

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
    MapPrefs map_prefs_of(const std::string& username) const;
    // True once the person has a saved mapp record (they saved Map settings, or the 2026-11-10 friends-on migration
    // wrote their defaults). Without one, map_prefs_of() gives MapPrefs{} defaults.
    bool has_map_prefs(const std::string& username) const;
    // Every account's username, sorted (suggestions and one-time migrations walk them).
    std::vector<std::string> all_usernames() const;
    // Turning MapPrefs::pains off deletes every pain point the person had.
    void set_map_prefs(const std::string& username, const MapPrefs& p);
    // A new suggestion; 0 when the owner/phrase/source is empty or the same
    // owner+phrase+source is already there.
    std::uint64_t add_pain_suggestion(const std::string& owner, const PainHit& hit, const std::string& source);
    std::vector<PainPoint> pains_of(const std::string& owner) const; // newest first
    std::optional<PainPoint> pain_by_id(std::uint64_t id) const;
    // Kept and Flowed rows with this subject, newest first. Still rows and
    // suggestions are left out. This is not a count.
    std::vector<PainPoint> flowed_with_subject(const std::string& phrase) const;
    bool keep_pain(const std::string& owner, std::uint64_t id);   // owner only
    // Flow (on) or Still (!on). Owner only, and only a kept box. A suggestion
    // stays private. The same call again changes nothing.
    bool flow_pain(const std::string& owner, std::uint64_t id, bool on);
    bool forget_pain(const std::string& owner, std::uint64_t id); // owner only; deletes it
    void clear_pains(const std::string& owner);

    bool is_findable_exact(const std::string& requester, const std::string& target) const;
    std::vector<std::string> typeahead(const std::string& requester, const std::string& prefix) const;

    std::uint64_t add_post(const std::string& author, const std::string& text, const std::string& community,
                           const std::string& repost_of_author = "", std::uint64_t repost_of_id = 0,
                           const std::string& attachment_blob_id = "", const std::string& attachment_mime = "",
                           PostAudience audience = PostAudience::Friends,
                           std::vector<std::string> exceptions = {},
                           std::vector<std::string> friend_allowlist = {},
                           std::vector<std::string> extra_groups = {});
    // Returns false (no-op) if the post doesn't exist or actor isn't its
    // author -- the caller (http_server, via its own post_by_id lookup)
    // decides which of those two cases it was, for a 404 vs. 403 response;
    // this is just the defense-in-depth check at the store layer itself.
    // Drops the post's own replies and second-lines too; leaves blob bytes
    // alone (content-addressed, may be shared) and leaves any existing
    // reposts of this id untouched -- a repost already carries its own copy
    // of the text (see repost_of_author's comment), so it keeps rendering
    // fine with the original simply gone from feeds.
    bool delete_post(std::uint64_t post_id, const std::string& actor);
    std::vector<Post> feed_for(const std::string& user, std::size_t limit) const;
    // Friends is exactly feed_for. All and Committees additionally pull in
    // posts from communities `user` actually belongs to -- "committees,"
    // not every community on the site. feed_for itself now also includes a
    // "Friends + Groups" combo post (audience==Friends with a community
    // set), same as any other Friends post; a GroupsOnly post never
    // appears in either feed_for or feed_view's "personal" half. (There was
    // a Drawings view, the posts with a media attachment; Brandon dropped it
    // 2026-11-10.)
    std::vector<Post> feed_view(const std::string& user, std::size_t limit, FeedFilter filter) const;
    std::vector<Post> posts_in(const std::string& community, std::size_t limit) const;
    std::vector<Post> posts_by(const std::string& author) const;
    // Operator panel only -- every post, unscoped by viewer/friendship/
    // community membership (every other post accessor here is deliberately
    // scoped; this one isn't, on purpose, since it's gated by is_operator()
    // at the HTTP layer, not by who could see the post normally).
    std::vector<Post> recent_posts(std::size_t limit) const;
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

    std::uint64_t add_proposal(const std::string& author, const std::string& title, const std::string& body,
                               BallotSpec spec = {});
    std::vector<Proposal> all_proposals() const; // oldest first, same as posts_by
    std::optional<Proposal> proposal_by_id(std::uint64_t id) const;
    // Proposals whose city key equals `key`, oldest first.
    std::vector<Proposal> proposals_in_city(const std::string& key) const;
    // Casting again overwrites the voter's own prior choice on this
    // proposal -- one live vote per (proposal, voter), not a running log.
    void cast_proposal_vote(std::uint64_t proposal_id, const std::string& voter, ProposalVote choice);
    std::optional<ProposalVote> proposal_vote_of(std::uint64_t proposal_id, const std::string& voter) const;
    ProposalTally tally_of(std::uint64_t proposal_id) const;
    // Number/Date/Choice/Scale. Payload meaning depends on kind (decimal,
    // YYYY-MM-DD, option index, 0-100). Invalid payload is a no-op.
    // False (nothing stored) when the proposal is missing, Yes/No, or the payload is refused (Brief 7 item 5: the page
    // says "Your answer didn't save").
    bool cast_ballot_value(std::uint64_t proposal_id, const std::string& voter, const std::string& payload);
    std::optional<std::string> ballot_value_of(std::uint64_t proposal_id, const std::string& voter) const;
    BallotTally value_tally_of(std::uint64_t proposal_id) const;
    // Append a Choice option. Author: unlimited (up to 48). Anyone else: at most one
    // on this proposal, and only if open_choice. Not a vote. Duplicate/empty = false.
    bool add_proposal_option(std::uint64_t proposal_id, const std::string& username, const std::string& label);
    bool mark_proposal_office(std::uint64_t proposal_id, const std::string& jurisdiction);
    // Tie a proposal to a city groups already name, or clear it (empty text).
    // Stores the city key. A city no group uses is refused and not written.
    bool set_proposal_city(std::uint64_t proposal_id, const std::string& city);
    // Set or clear the "who could act on it" line. Same tidy rules as a city name.
    bool set_proposal_actor(std::uint64_t proposal_id, const std::string& who);
    // Add a department or council to a city. The actor must be a member of one
    // of that city's groups. Empty text is refused. A case-only repeat is kept
    // as the first spelling and still returns true.
    bool add_city_tag(const std::string& key, const std::string& actor, const std::string& tag);
    std::vector<std::string> city_tags(const std::string& key) const;
    // Operator moderation (2026-11-08): every (city key, tag) pair, cities in key order, tags in list order.
    std::vector<std::pair<std::string, std::string>> all_city_tags() const;
    // Ids of the proposals that name this tag (case-insensitive), for the moderation list and archive.
    std::vector<std::uint64_t> proposals_with_city_tag(const std::string& key, const std::string& tag) const;
    // Remove one department/council from a city (case-insensitive match) and drop it from every proposal that named
    // it. False when the city has no such tag. Persists at once.
    bool delete_city_tag(const std::string& key, const std::string& tag);
    // Keep the wanted tags that this proposal's city actually has, in city-list
    // order. Unknown tags are dropped. A proposal with no city keeps none.
    bool set_proposal_tags(std::uint64_t proposal_id, const std::vector<std::string>& tags);
    // Operator moderation (Brandon 2026-11-08: "people are abusing it"). delete_proposal removes the proposal and
    // everything keyed to it (comments, reactions, votes, ballot answers, problem suggestions pointing at it);
    // edit_proposal replaces only the title and body. Both false when the id is unknown or the new title is empty.
    bool delete_proposal(std::uint64_t proposal_id);
    // Operator moderation of single comments and of older posts (Brandon 2026-11-08, "see if you can get more done").
    std::vector<Post> all_posts() const;                                         // oldest first
    std::vector<PostReply> recent_post_replies(std::size_t limit) const;         // newest first
    std::vector<ProposalReply> recent_proposal_replies(std::size_t limit) const; // newest first
    std::optional<PostReply> post_reply_by_id(std::uint64_t id) const;
    std::optional<ProposalReply> proposal_reply_by_id(std::uint64_t id) const;
    bool delete_post_reply(std::uint64_t reply_id);     // false when unknown
    bool delete_proposal_reply(std::uint64_t reply_id); // false when unknown
    bool edit_proposal(std::uint64_t proposal_id, const std::string& title, const std::string& body);
    // Comment. Empty text or a missing proposal returns 0. Text is trimmed,
    // other control characters dropped, and cut at 1000 bytes on a UTF-8 boundary.
    std::uint64_t add_proposal_reply(std::uint64_t proposal_id, const std::string& author,
                                     const std::string& text);
    std::vector<ProposalReply> proposal_replies_of(std::uint64_t proposal_id) const;
    // One reaction per person. Empty kind removes. Unknown kind returns false
    // and changes nothing. The set is support, oppose, question, noted.
    bool set_proposal_reaction(std::uint64_t proposal_id, const std::string& user,
                               const std::string& kind);
    std::optional<std::string> proposal_reaction_of(std::uint64_t proposal_id,
                                                    const std::string& user) const;
    // Always the four kinds, in that order, with a count (zero included).
    std::vector<std::pair<std::string, int>> proposal_reaction_counts(std::uint64_t proposal_id) const;

    bool create_community(const Community& c);
    std::optional<Community> community(const std::string& slug) const;
    std::vector<Community> all_communities() const;
    // Distinct, non-empty subcategory names already in use under a
    // category, sorted -- for suggesting an existing one back to whoever's
    // creating the next community there instead of always minting a new
    // subcategory name for the same idea.
    std::vector<std::string> subcategories_of(CommunityCategory category) const;
    // Every city at least one group names, one entry per city_key(), sorted
    // by name -- for the city typeahead and the Groups page's city list.
    std::vector<CityUse> cities_in_use() const;
    // Groups whose city_key() equals `key`, sorted by name.
    std::vector<Community> communities_in_city(const std::string& key) const;
    // Set or clear (empty) a group's city. Only the group's owner may. The
    // text goes through clean_city(); text with no letters or digits clears.
    bool set_community_city(const std::string& slug, const std::string& actor, const std::string& city);
    // True when `username` is a member of at least one group whose city_key is `key`.
    bool in_city(const std::string& key, const std::string& username) const;
    // Opt in or out of the city's "People here" list. Only a current member
    // of one of that city's groups can opt in. Absence is not listed.
    bool set_city_listed(const std::string& key, const std::string& username, bool on);
    bool is_city_listed(const std::string& key, const std::string& username) const;
    // Members of this city's groups who opted in, sorted by username.
    std::vector<std::string> people_listed_in_city(const std::string& key) const;
    bool is_member(const std::string& slug, const std::string& username) const;
    bool join_community(const std::string& slug, const std::string& username);
    bool invite_community(const std::string& slug, const std::string& actor, const std::string& target);
    void leave_community(const std::string& slug, const std::string& username);
    // Deletes the community and its memberships. Does NOT touch posts made
    // into it -- they keep their community slug (now a dead link, /c/<slug>
    // simply 404s) rather than being cascade-deleted, so closing a room
    // never silently erases what people said in it. `actor` must be the
    // community's own owner; the operator panel's delete route satisfies
    // this by passing the real owner through, not by bypassing the check.
    bool delete_community(const std::string& slug, const std::string& actor);
    std::vector<std::string> members_of(const std::string& slug) const;
    std::vector<Community> communities_of(const std::string& username) const;

    void add_notice(const std::string& to, const std::string& type, const std::string& from,
                    const std::string& extra);
    std::vector<Notice> notices_for(const std::string& user, std::size_t limit) const;
    int unread_count(const std::string& user) const;
    // "message" notices badge the header's Messages icon separately from
    // everything else (friend_request/friend_accept/group_invite) badging
    // the Notifications bell -- unread_count(user) minus this is the
    // notifications-only count.
    int unread_message_count(const std::string& user) const;
    void mark_notices_read(const std::string& user);
    // Split so opening Mail doesn't silently clear a pending friend-request
    // notice, and visiting Notices doesn't silently clear an unread message.
    void mark_notices_read_of_type(const std::string& user, const std::string& type);
    void mark_notices_read_except_type(const std::string& user, const std::string& except_type);

    void set_phone_fp(AccountId id, const std::string& fp);
    bool phone_quarantined(const std::string& fp) const;
    bool delete_account(AccountId id);

    // Operator panel: account lock (see the "lk" tag's own comment).
    bool is_locked(const std::string& username) const override;
    // Locking revokes every existing session for the account immediately
    // (same reasoning as a password change); unlocking does not restore
    // them -- signing back in issues a fresh one, same as any other login.
    void set_locked(const std::string& username, bool locked);
    void add_audit_log(const std::string& operator_name, const std::string& action,
                       const std::string& target);
    // Newest first, capped at `limit` -- /admin/log shows the tail of this.
    std::vector<AuditLogEntry> recent_audit_log(std::size_t limit) const;

    // Program sending (see ProgramGrant and the "prgr" tag). Setting a grant
    // with no kinds removes it. Deleting the account removes it too.
    std::optional<ProgramGrant> program_grant(const std::string& username) const;
    void set_program_grant(const ProgramGrant& grant);
    void revoke_program_grant(const std::string& username);
    // The download name of a program attachment ("afn" tag), already
    // sanitized by the caller; other attachments keep the mime-derived name.
    // Keyed per message (attachment_name_key below): blobs are content-
    // addressed, so two sends of the same bytes share one blob.
    void set_attachment_name(const std::string& key, const std::string& name);
    std::optional<std::string> attachment_name(const std::string& key) const;

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
    std::int64_t last_group_ts(ConversationId conv_id) const;
    std::string last_group_preview(ConversationId conv_id) const;
    std::optional<Message> find_group_attachment(AccountId account, const std::string& blob_id) const;
    std::vector<Message> all_group_messages_with_attachments() const;

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
    void skip_jukebox_track(const std::string& owner, const std::string& video_id);
    void record_jukebox_play(const std::string& owner, const std::string& video_id, const std::string& queued_by);
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

    // "Play my Juketube when someone opens my profile" -- off by default.
    bool jukebox_autoplay(const std::string& username) const;
    void set_jukebox_autoplay(const std::string& username, bool on);
    // "Play this Juketube instead" -- empty means no relay (own queue).
    std::string jukebox_relay_target(const std::string& username) const;
    void set_jukebox_relay_target(const std::string& username, const std::string& target);
    // Resolves username's relay at most one hop: a missing/self/blocked-back
    // target, or a target that itself relays straight back to username
    // (A->B->A), all fall back to username's own queue rather than being
    // chased further or left dangling. Callers that want "whose queue is
    // actually playing here" call this instead of using username directly.
    std::string jukebox_effective_owner(const std::string& username) const;

    // The shared playhead freeze point for a station -- see the jpos tag
    // comment. std::nullopt means no freeze on record (station never
    // played, or has never had its last listener leave since this existed).
    std::optional<JukeboxPos> jukebox_pos(const std::string& station) const;
    void set_jukebox_pos(const std::string& station, const std::string& video_id,
                          std::int64_t offset_sec, std::int64_t updated_at);
    // Drops the saved resume point. The next join must not start that video again.
    void clear_jukebox_pos(const std::string& station);

    const std::string& path() const { return path_; }

private:
    void load();
    void flush();
    // The LGP1 load and save, split by record module (src/records/<module>.cpp; 2026-11-12). load() offers each line
    // to every read_* until one claims its type, then runs the finish_* in a fixed order; flush() calls the write_*
    // sections in the order the store has always been written. Both orders live in src/file_store.cpp only.
    struct LoadPending; // rows parked until every line is read (src/core/load_pending.hpp)
    bool read_accounts(const std::vector<std::string>& f, LoadPending& pending);
    bool read_admin(const std::vector<std::string>& f, LoadPending& pending);
    bool read_avatars(const std::vector<std::string>& f, LoadPending& pending);
    bool read_city(const std::vector<std::string>& f, LoadPending& pending);
    bool read_communities(const std::vector<std::string>& f, LoadPending& pending);
    bool read_conversations(const std::vector<std::string>& f, LoadPending& pending);
    bool read_counters(const std::vector<std::string>& f, LoadPending& pending);
    bool read_jukebox(const std::vector<std::string>& f, LoadPending& pending);
    bool read_mail(const std::vector<std::string>& f, LoadPending& pending);
    bool read_notices(const std::vector<std::string>& f, LoadPending& pending);
    bool read_pains(const std::vector<std::string>& f, LoadPending& pending);
    bool read_posts(const std::vector<std::string>& f, LoadPending& pending);
    bool read_prefs(const std::vector<std::string>& f, LoadPending& pending);
    bool read_profile(const std::vector<std::string>& f, LoadPending& pending);
    bool read_here(const std::vector<std::string>& f, LoadPending& pending);
    bool read_styles(const std::vector<std::string>& f, LoadPending& pending);
    bool read_proposal_comments(const std::vector<std::string>& f, LoadPending& pending);
    bool read_proposals(const std::vector<std::string>& f, LoadPending& pending);
    bool read_social(const std::vector<std::string>& f, LoadPending& pending);
    void finish_city_lists(LoadPending& pending);
    void finish_city_tags(LoadPending& pending);
    void finish_communities(LoadPending& pending);
    void finish_posts(LoadPending& pending);
    void finish_proposal_comments(LoadPending& pending);
    void finish_proposals(LoadPending& pending);
    void finish_here();
    void finish_styles();
    void write_accounts(std::ostream& body);
    void write_admin(std::ostream& body);
    void write_attachment_names(std::ostream& body);
    void write_avatars(std::ostream& body);
    void write_blocks(std::ostream& body);
    void write_author_css(std::ostream& body);
    void write_canvas(std::ostream& body);
    void write_here(std::ostream& body);
    void write_styles(std::ostream& body);
    void forget_styles_of(const std::string& name);   // account deletion (records/styles.cpp)
    void write_city_lists(std::ostream& body);
    void write_city_tags(std::ostream& body);
    void write_communities(std::ostream& body);
    void write_conversations(std::ostream& body);
    void write_counters(std::ostream& body);
    void write_friend_links(std::ostream& body);
    void write_jukebox(std::ostream& body);
    void write_mail(std::ostream& body);
    void write_mail_read(std::ostream& body);
    void write_map_prefs(std::ostream& body);
    void write_members(std::ostream& body);
    void write_notices(std::ostream& body);
    void write_pains(std::ostream& body);
    void write_posts(std::ostream& body);
    void write_prefs(std::ostream& body);
    void write_profiles(std::ostream& body);
    void write_proposal_comments(std::ostream& body);
    void write_proposals(std::ostream& body);
    void drop_requests_between(const std::string& a, const std::string& b);
    void strip_inner_both(const std::string& a, const std::string& b);
    // Shared by feed_for/feed_view: whether a confirmed, non-blocked friend
    // gets to see this Friends/Anybody post at all. See its own comment in
    // src/records/posts.cpp.
    bool friend_sees_post(const Post& p, const std::string& viewer) const;

    std::string path_;
    AccountId next_id_ = 1;
    std::uint64_t next_post_id_ = 1;
    std::uint64_t next_note_id_ = 1;
    std::map<std::string, AccountId> by_username_;
    std::map<AccountId, PlatformAccount> accounts_;
    std::set<std::string> locked_;
    std::vector<AuditLogEntry> audit_log_;
    std::map<std::string, ProgramGrant> program_grants_;
    std::map<std::string, std::string> attachment_names_; // attachment_name_key -> download name
    // Drops names whose message, or an account in it, is gone ("afn").
    void prune_attachment_names();
    std::map<SessionId, Session> sessions_;
    std::map<AccountId, std::string> blurbs_;
    std::map<std::string, Profile> profiles_;
    std::map<AccountId, std::string> author_css_;
    std::map<std::string, bool> here_choice_;   // here: username -> shows as a dot
    std::map<std::uint64_t, ProfileStyle> styles_;                                  // sty
    std::uint64_t next_style_id_ = 1;                                               // styn
    std::map<std::uint64_t, std::map<std::string, std::int64_t>> style_uses_;      // stu: id -> person -> ts
    std::map<std::uint64_t, std::map<std::string, StyleReport>> style_reports_;    // str: id -> reporter -> row
    std::map<AccountId, std::vector<std::string>> canvas_frames_;
    std::map<AccountId, bool> canvas_timelapse_;
    std::map<AccountId, std::string> phone_fp_;
    std::map<std::string, std::int64_t> quarantine_;
    std::map<std::string, std::string> emails_; // username -> deliverable email (see ml tag)
    std::map<std::string, ResetToken> reset_tokens_; // username -> outstanding token (see pr tag)
    std::map<std::string, std::string> google_ids_; // username -> Google sub (see gid tag)
    std::set<std::pair<std::string, std::string>> friends_;
    std::vector<std::pair<std::string, std::string>> requests_;
    std::vector<MailMessage> mail_;
    std::set<std::pair<std::string, std::string>> blocks_;
    std::map<std::pair<std::string, std::string>, BlockEntry> block_meta_;
    std::map<std::string, std::set<std::string>> inner_;
    std::map<std::string, AccountPrefs> prefs_;
    std::map<std::string, MapPrefs> map_prefs_; // absent = MapPrefs{} defaults
    std::vector<Post> posts_;
    std::uint64_t next_reply_id_ = 1;
    std::vector<PostReply> replies_;
    std::set<std::pair<std::uint64_t, std::string>> seconds_;
    std::uint64_t next_proposal_id_ = 1;
    std::vector<Proposal> proposals_;
    std::uint64_t next_proposal_reply_id_ = 1;
    std::vector<ProposalReply> proposal_replies_;
    std::vector<PainPoint> pains_;
    std::uint64_t next_pain_id_ = 1;
    void prune_pains(); // expired suggestions, and ones whose source is gone
    // proposal id -> username -> kind
    std::map<std::uint64_t, std::map<std::string, std::string>> proposal_reactions_;
    std::map<std::uint64_t, std::map<std::string, ProposalVote>> proposal_votes_;
    std::map<std::uint64_t, std::map<std::string, std::string>> ballot_values_;
    std::map<std::string, Community> communities_;
    std::map<std::string, std::set<std::string>> members_;
    // city_key -> usernames who asked to be listed on that city's page.
    std::map<std::string, std::set<std::string>> city_listed_;
    // city_key -> department/council tags, first spelling, insertion order.
    bool remember_city_tag(const std::string& key, const std::string& raw);
    std::map<std::string, std::vector<std::string>> city_tags_;
    std::vector<Notice> notices_;
    std::map<AccountId, Avatar> avatars_;
    std::map<std::string, std::vector<JukeboxTrack>> jukebox_;
    std::map<std::string, std::vector<JukeboxHistoryEntry>> jukebox_history_;
    std::set<std::pair<std::string, std::string>> jukebox_subs_; // (subscriber, owner)
    std::map<std::string, bool> jukebox_autoplay_;
    std::map<std::string, std::string> jukebox_relay_;
    std::map<std::string, JukeboxPos> jukebox_pos_; // keyed by station id
    ConversationId next_conv_id_ = 1;
    MessageId next_message_id_ = 1;
    std::map<ConversationId, Conversation> conversations_;
    std::map<MessageId, Message> messages_;
    std::map<std::string, std::map<std::string, std::int64_t>> mail_last_read_;
};

} // namespace liquidgov::platform
