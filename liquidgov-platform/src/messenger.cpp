#include "liquidgov/platform/messenger.hpp"

#include "liquidgov/platform/clock.hpp"

#include <algorithm>

namespace liquidgov::platform {

bool violates_minor_invariant(const SocialGraph& graph, AccountId candidate,
                               const std::vector<ConversationMember>& existing_members) {
    for (const auto& member : existing_members) {
        if (member.account == candidate) continue; // already present, not a new pairing
        bool candidate_is_minor = graph.is_minor(candidate);
        bool member_is_minor = graph.is_minor(member.account);
        if ((candidate_is_minor || member_is_minor) &&
            !graph.are_friends(candidate, member.account)) {
            return true;
        }
    }
    return false;
}

CreateConversationResult create_conversation(ConversationStore& store, const SocialGraph& graph,
                                              AccountId initiator, std::vector<AccountId> invitees,
                                              bool is_group) {
    std::vector<ConversationMember> members = {{initiator, MemberState::Active}};

    for (AccountId invitee : invitees) {
        if (violates_minor_invariant(graph, invitee, members)) {
            continue; // silently excluded from THIS creation — see header note
        }
        MemberState state =
            graph.are_friends(initiator, invitee) ? MemberState::Active : MemberState::Pending;
        members.push_back({invitee, state});
    }

    Conversation conv;
    conv.members = members;
    conv.is_group = is_group;
    conv.created_at = now();
    conv.id = store.create_conversation(conv);

    return {MessengerError::None, conv.id};
}

AddMemberResult add_member(ConversationStore& store, const SocialGraph& graph, AccountId actor,
                            AccountId candidate, ConversationId conv_id) {
    auto conv = store.get_conversation(conv_id);
    if (!conv) return {MessengerError::ConversationNotFound};

    if (violates_minor_invariant(graph, candidate, conv->members)) {
        // Hard refuse — no roster mutation. Same generic failure
        // regardless of whether the cause is the minor invariant or,
        // later, an adult "ask before adding me" preference — that
        // shared failure shape is what keeps this from being an age
        // oracle.
        return {MessengerError::ContactSettingsConflict};
    }

    MemberState state =
        graph.are_friends(actor, candidate) ? MemberState::Active : MemberState::Pending;
    conv->members.push_back({candidate, state});
    store.save_conversation(*conv);

    return {MessengerError::None};
}

SendMessageResult send_message(ConversationStore& store, AccountId sender, ConversationId conv_id,
                                const std::string& text) {
    auto conv = store.get_conversation(conv_id);
    if (!conv) return {MessengerError::ConversationNotFound, std::nullopt};

    bool is_active = std::any_of(conv->members.begin(), conv->members.end(), [&](const auto& m) {
        return m.account == sender && m.state == MemberState::Active;
    });
    if (!is_active) return {MessengerError::NotAMember, std::nullopt};

    Message msg;
    msg.conversation_id = conv_id;
    msg.sender = sender;
    msg.sent_at = now();
    msg.text = text;
    msg.id = store.save_message(msg);

    return {MessengerError::None, msg.id};
}

void on_relationship_ended(ConversationStore& store, AccountId initiator, AccountId other_party) {
    for (auto conv : store.conversations_for(initiator)) {
        bool shares_with_other = std::any_of(
            conv.members.begin(), conv.members.end(),
            [&](const auto& m) { return m.account == other_party; });
        if (!shares_with_other) continue;

        // Age-blind, uniform: the initiator leaves, full stop — no
        // branch on who other_party is or what their minor status is.
        conv.members.erase(std::remove_if(conv.members.begin(), conv.members.end(),
                                           [&](const auto& m) { return m.account == initiator; }),
                            conv.members.end());
        store.save_conversation(conv);
    }
}

MessengerActionResult edit_message(ConversationStore& store, AccountId editor, MessageId id,
                                    const std::string& new_text) {
    auto msg = store.get_message(id);
    if (!msg) return {MessengerError::MessageNotFound};
    if (msg->sender != editor) return {MessengerError::NotYourMessage};
    if (msg->deletion != DeletionState::NotDeleted) return {MessengerError::AlreadyDeleted};

    msg->edit_history.push_back(msg->text); // kept, never overwritten silently
    msg->text = new_text;
    msg->edited_at = now();
    store.update_message(*msg);

    return {MessengerError::None};
}

MessengerActionResult delete_message(ConversationStore& store, AccountId requester, MessageId id,
                                      bool for_everyone) {
    auto msg = store.get_message(id);
    if (!msg) return {MessengerError::MessageNotFound};
    if (for_everyone && msg->sender != requester) return {MessengerError::NotYourMessage};

    msg->deletion = for_everyone ? DeletionState::DeletedForEveryone : DeletionState::DeletedForSender;
    msg->deleted_at = now();
    store.update_message(*msg);

    return {MessengerError::None};
}

void report_message(ConversationStore& store, MessageId id) {
    auto msg = store.get_message(id);
    if (!msg) return;
    msg->under_review = true;
    store.update_message(*msg);
}

void purge_expired_deletions(ConversationStore& store, std::int64_t now_ts,
                              std::int64_t retention_window_seconds) {
    for (auto msg : store.all_messages()) {
        if (msg.under_review) continue; // report in flight — window does not apply
        if (msg.deletion != DeletionState::DeletedForEveryone) continue;
        if (now_ts - msg.deleted_at > retention_window_seconds) {
            // "Purge" here means the retained content is gone; a real
            // deployment would hard-delete the row. This module just
            // marks it, since ConversationStore has no delete primitive
            // of its own — the point being proven is the window +
            // under_review gate, not row deletion mechanics.
            msg.text = "";
            msg.edit_history.clear();
            store.update_message(msg);
        }
    }
}

} // namespace liquidgov::platform
