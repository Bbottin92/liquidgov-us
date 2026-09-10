#pragma once

#include <optional>
#include <string>
#include <vector>

#include "liquidgov/platform/conversation.hpp"
#include "liquidgov/platform/social_graph.hpp"

namespace liquidgov::platform {

class ConversationStore {
public:
    virtual ~ConversationStore() = default;
    virtual ConversationId create_conversation(const Conversation& conv) = 0;
    virtual std::optional<Conversation> get_conversation(ConversationId id) const = 0;
    virtual void save_conversation(const Conversation& conv) = 0;
    virtual std::vector<Conversation> conversations_for(AccountId account) const = 0;

    virtual MessageId save_message(const Message& msg) = 0;
    virtual std::optional<Message> get_message(MessageId id) const = 0;
    virtual void update_message(const Message& msg) = 0;
    virtual std::vector<Message> all_messages() const = 0;
};

enum class MessengerError {
    None,
    ContactSettingsConflict,
    ConversationNotFound,
    NotAMember,
    MessageNotFound,
    NotYourMessage,
    AlreadyDeleted,
};

// A conversation may never contain a minor and a non-friend of that
// minor simultaneously. Symmetric by construction — it does not matter
// which side is "joining": a stranger added to a room with an existing
// minor and a minor added to a room with an existing stranger are the
// same check.
bool violates_minor_invariant(const SocialGraph& graph, AccountId candidate,
                               const std::vector<ConversationMember>& existing_members);

struct CreateConversationResult {
    MessengerError error = MessengerError::None;
    std::optional<ConversationId> conversation_id;
};

// New-conversation creation SILENTLY EXCLUDES any invitee that would
// violate the invariant against members already accepted into the
// initial set — no Pending record for them, nothing for anyone to
// stumble into or be notified about. This is deliberately different
// from add_member below: creating a group with a mix of friends and one
// incompatible stranger should not fail the whole group, it should just
// not include that one person.
CreateConversationResult create_conversation(ConversationStore& store, const SocialGraph& graph,
                                              AccountId initiator, std::vector<AccountId> invitees,
                                              bool is_group);

struct AddMemberResult {
    MessengerError error = MessengerError::None;
};

// Adding to an EXISTING conversation is the opposite policy from
// create_conversation: a violating add is HARD REFUSED, no roster
// mutation at all. The inviter's path forward is forking a new
// conversation, not an override of this one.
AddMemberResult add_member(ConversationStore& store, const SocialGraph& graph, AccountId actor,
                            AccountId candidate, ConversationId conv_id);

struct SendMessageResult {
    MessengerError error = MessengerError::None;
    std::optional<MessageId> message_id;
};

// Only Active members may send. A Pending member (not yet accepted)
// cannot send until they transition to Active.
SendMessageResult send_message(ConversationStore& store, AccountId sender, ConversationId conv_id,
                                const std::string& text);

// Unfriend AND block use this same function — age-blind, uniform: the
// initiator always leaves every conversation shared with the other
// party, regardless of who that party is. This uniformity is what
// actually closes the enumeration leak an earlier, asymmetric
// "protect the minor" version had: the observable behavior (who leaves)
// never differs based on the other party's minor status, because it
// never depends on it at all.
void on_relationship_ended(ConversationStore& store, AccountId initiator, AccountId other_party);

struct MessengerActionResult {
    MessengerError error = MessengerError::None;
};

// Edits are never silent — the prior text is kept in edit_history, not
// overwritten. A visible edit history is what stops "send something,
// get a reaction, quietly change it to something else."
MessengerActionResult edit_message(ConversationStore& store, AccountId editor, MessageId id,
                                    const std::string& new_text);

// delete-for-everyone requires being the sender; delete-for-sender does
// not (it only ever affects the requester's own view). Either way this
// HIDES, it does not purge — see purge_expired_deletions.
MessengerActionResult delete_message(ConversationStore& store, AccountId requester, MessageId id,
                                      bool for_everyone);

// Marks a message as under active moderation review. Snapshots nothing
// itself — it exists purely to exempt the message from the retention
// purge below regardless of when it was deleted.
void report_message(ConversationStore& store, MessageId id);

// Purges DeletedForEveryone messages whose retention window has elapsed,
// unless under_review. Exposed as a pure function over an explicit "now"
// so it is testable without a real clock dependency; a real deployment
// runs this on a schedule.
void purge_expired_deletions(ConversationStore& store, std::int64_t now,
                              std::int64_t retention_window_seconds);

// The only way Pending ever becomes Active — nothing else in this file
// promotes a member. Only the pending member themselves calls this for
// their own AccountId; the caller (site layer) is responsible for that
// authorization check the same way it is for every other actor param
// here. Idempotent if already Active.
MessengerActionResult accept_invite(ConversationStore& store, AccountId account,
                                     ConversationId conv_id);

// Removes account from the conversation outright. This is how a Pending
// member declines — never notified, per the closed "no
// FriendRequestDeclined" pin extended to groups — and how an Active
// member leaves voluntarily. Same function either way; the caller does
// not need to know which case it is.
MessengerActionResult leave_conversation(ConversationStore& store, AccountId account,
                                          ConversationId conv_id);

} // namespace liquidgov::platform
