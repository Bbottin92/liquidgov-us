#include "liquidgov/platform/messenger.hpp"

#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <utility>

using namespace liquidgov::platform;

namespace {

class FakeSocialGraph : public SocialGraph {
public:
    void set_friends(AccountId a, AccountId b) {
        friend_pairs_.insert(key(a, b));
    }
    void set_minor(AccountId a) { minors_.insert(a); }

    bool are_friends(AccountId a, AccountId b) const override {
        return friend_pairs_.count(key(a, b)) > 0;
    }
    bool is_minor(AccountId account) const override { return minors_.count(account) > 0; }

private:
    static std::pair<AccountId, AccountId> key(AccountId a, AccountId b) {
        return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
    }
    std::set<std::pair<AccountId, AccountId>> friend_pairs_;
    std::set<AccountId> minors_;
};

class InMemoryConversationStore : public ConversationStore {
public:
    ConversationId create_conversation(const Conversation& conv) override {
        ConversationId id = next_conv_id_++;
        Conversation stored = conv;
        stored.id = id;
        conversations_[id] = stored;
        return id;
    }
    std::optional<Conversation> get_conversation(ConversationId id) const override {
        auto it = conversations_.find(id);
        if (it == conversations_.end()) return std::nullopt;
        return it->second;
    }
    void save_conversation(const Conversation& conv) override { conversations_[conv.id] = conv; }
    std::vector<Conversation> conversations_for(AccountId account) const override {
        std::vector<Conversation> out;
        for (const auto& [id, conv] : conversations_) {
            for (const auto& m : conv.members) {
                if (m.account == account) {
                    out.push_back(conv);
                    break;
                }
            }
        }
        return out;
    }

    MessageId save_message(const Message& msg) override {
        MessageId id = next_msg_id_++;
        Message stored = msg;
        stored.id = id;
        messages_[id] = stored;
        return id;
    }
    std::optional<Message> get_message(MessageId id) const override {
        auto it = messages_.find(id);
        if (it == messages_.end()) return std::nullopt;
        return it->second;
    }
    void update_message(const Message& msg) override { messages_[msg.id] = msg; }
    std::vector<Message> all_messages() const override {
        std::vector<Message> out;
        for (const auto& [id, m] : messages_) out.push_back(m);
        return out;
    }

private:
    ConversationId next_conv_id_ = 1;
    MessageId next_msg_id_ = 1;
    std::map<ConversationId, Conversation> conversations_;
    std::map<MessageId, Message> messages_;
};

bool conversation_has_member(const Conversation& conv, AccountId account) {
    for (const auto& m : conv.members) {
        if (m.account == account) return true;
    }
    return false;
}

MemberState state_of(const Conversation& conv, AccountId account) {
    for (const auto& m : conv.members) {
        if (m.account == account) return m.state;
    }
    throw std::runtime_error("not a member");
}

} // namespace

int main() {
    // ---- Active/Pending gating on creation ----
    {
        InMemoryConversationStore store;
        FakeSocialGraph graph;
        graph.set_friends(1, 2); // 1 and 2 are friends; 1 and 3 are not

        auto result = create_conversation(store, graph, /*initiator=*/1, {2, 3}, /*is_group=*/true);
        assert(result.error == MessengerError::None);
        auto conv = store.get_conversation(*result.conversation_id);
        assert(state_of(*conv, 2) == MemberState::Active);  // friend -> Active
        assert(state_of(*conv, 3) == MemberState::Pending); // stranger -> Pending
        std::cout << "create_conversation: friends Active, strangers Pending\n";
    }

    // ---- Minor invariant, create-time: silent exclusion, not a hard failure ----
    {
        InMemoryConversationStore store;
        FakeSocialGraph graph;
        graph.set_minor(99); // account 99 is a minor
        graph.set_friends(1, 2);
        // 1 and 99 are NOT friends. Inviting {2, 99} should silently
        // drop 99 while still creating the conversation with 2.
        auto result = create_conversation(store, graph, /*initiator=*/1, {2, 99}, true);
        assert(result.error == MessengerError::None); // whole creation does NOT fail
        auto conv = store.get_conversation(*result.conversation_id);
        assert(conversation_has_member(*conv, 2));
        assert(!conversation_has_member(*conv, 99)); // silently excluded
        std::cout << "create_conversation: minor-incompatible invitee silently excluded, "
                     "rest of the group still forms\n";
    }

    // ---- Minor invariant, add-time: hard refuse, no roster mutation ----
    {
        InMemoryConversationStore store;
        FakeSocialGraph graph;
        graph.set_minor(99);
        graph.set_friends(1, 99); // 1 and 99 ARE friends, so 99 can be in the group
        auto created = create_conversation(store, graph, 1, {99}, true);
        auto conv_before = store.get_conversation(*created.conversation_id);
        std::size_t member_count_before = conv_before->members.size();

        // Account 50 is a stranger to 99 (the minor already in the room).
        // Adding 50 must be hard-refused.
        auto add_result = add_member(store, graph, /*actor=*/1, /*candidate=*/50,
                                      *created.conversation_id);
        assert(add_result.error == MessengerError::ContactSettingsConflict);

        auto conv_after = store.get_conversation(*created.conversation_id);
        assert(conv_after->members.size() == member_count_before); // NO roster mutation
        std::cout << "add_member: hard refuse on minor invariant, roster untouched\n";

        // A friend of 99, though, CAN be added.
        graph.set_friends(99, 60);
        auto add_ok = add_member(store, graph, 1, 60, *created.conversation_id);
        assert(add_ok.error == MessengerError::None);
        std::cout << "add_member: a candidate who IS friends with the minor succeeds\n";

        auto outsider = add_member(store, graph, /*actor=*/50, 70, *created.conversation_id);
        assert(outsider.error == MessengerError::NotAMember);
        std::cout << "add_member: a non-Active actor is refused\n";
    }

    // ---- send_message: Pending cannot send, Active can ----
    {
        InMemoryConversationStore store;
        FakeSocialGraph graph; // nobody is friends
        auto created = create_conversation(store, graph, 1, {2}, false);

        auto pending_send = send_message(store, /*sender=*/2, *created.conversation_id, "hi");
        assert(pending_send.error == MessengerError::NotAMember); // 2 is Pending, not Active
        std::cout << "send_message: a Pending member cannot send\n";

        auto active_send = send_message(store, /*sender=*/1, *created.conversation_id, "hi");
        assert(active_send.error == MessengerError::None);
        std::cout << "send_message: the Active initiator can send\n";
    }

    // ---- on_relationship_ended: age-blind, uniform, initiator always leaves ----
    {
        InMemoryConversationStore store;
        FakeSocialGraph graph;
        graph.set_friends(1, 2);
        graph.set_friends(1, 3);
        graph.set_minor(3); // one of these two cases involves a minor, one does not

        auto conv_with_adult = create_conversation(store, graph, 1, {2}, false);
        auto conv_with_minor = create_conversation(store, graph, 1, {3}, false);

        // Case A: initiator (1) unfriends the ADULT (2) -> 1 leaves that conversation.
        on_relationship_ended(store, /*initiator=*/1, /*other_party=*/2);
        auto after_a = store.get_conversation(*conv_with_adult.conversation_id);
        assert(!conversation_has_member(*after_a, 1)); // initiator left
        assert(conversation_has_member(*after_a, 2));  // other party stays

        // Case B: initiator (1) unfriends the MINOR (3) -> SAME outcome,
        // same mechanism, no branch on minor status.
        on_relationship_ended(store, /*initiator=*/1, /*other_party=*/3);
        auto after_b = store.get_conversation(*conv_with_minor.conversation_id);
        assert(!conversation_has_member(*after_b, 1));
        assert(conversation_has_member(*after_b, 3));

        std::cout << "on_relationship_ended: identical outcome whether the other party is a "
                     "minor or not — no observable difference to base an oracle on\n";
    }

    // ---- edit_message ----
    {
        InMemoryConversationStore store;
        FakeSocialGraph graph;
        auto conv = create_conversation(store, graph, 1, {}, false);
        auto sent = send_message(store, 1, *conv.conversation_id, "original text");

        auto wrong_editor = edit_message(store, /*editor=*/2, *sent.message_id, "hacked");
        assert(wrong_editor.error == MessengerError::NotYourMessage);

        auto ok_edit = edit_message(store, /*editor=*/1, *sent.message_id, "edited text");
        assert(ok_edit.error == MessengerError::None);
        auto msg = store.get_message(*sent.message_id);
        assert(msg->text == "edited text");
        assert(msg->edit_history.size() == 1);
        assert(msg->edit_history[0] == "original text"); // prior version kept, not lost
        std::cout << "edit_message: history preserved, wrong editor rejected\n";
    }

    // ---- delete_message + retention window + under_review exemption ----
    {
        InMemoryConversationStore store;
        FakeSocialGraph graph;
        auto conv = create_conversation(store, graph, 1, {}, false);

        auto msg_a = send_message(store, 1, *conv.conversation_id, "will be reported");
        auto msg_b = send_message(store, 1, *conv.conversation_id, "ordinary delete");

        // for_everyone by a non-sender is rejected
        auto bad_delete = delete_message(store, /*requester=*/2, *msg_a.message_id, true);
        assert(bad_delete.error == MessengerError::NotYourMessage);

        delete_message(store, 1, *msg_a.message_id, true);
        delete_message(store, 1, *msg_b.message_id, true);
        report_message(store, *msg_a.message_id); // msg_a is under review, msg_b is not

        std::int64_t deleted_at = store.get_message(*msg_a.message_id)->deleted_at;
        std::int64_t retention_window = 7 * 24 * 60 * 60; // 7 days, per the settled design

        // Purge "now" = well past the window for both messages.
        purge_expired_deletions(store, deleted_at + retention_window + 1, retention_window);

        auto a_after = store.get_message(*msg_a.message_id);
        auto b_after = store.get_message(*msg_b.message_id);
        assert(a_after->text == "will be reported"); // under_review survives the purge
        assert(b_after->text.empty());                // ordinary deletion is purged on schedule
        std::cout << "purge_expired_deletions: under_review survives the window, "
                     "an ordinary deletion is purged once it elapses\n";
    }

    // ---- accept_invite: the only path from Pending to Active ----
    {
        InMemoryConversationStore store;
        FakeSocialGraph graph; // nobody is friends -> invitee lands Pending
        auto conv = create_conversation(store, graph, 1, {2}, true);
        assert(state_of(*store.get_conversation(*conv.conversation_id), 2) == MemberState::Pending);

        auto wrong_account = accept_invite(store, /*account=*/3, *conv.conversation_id);
        assert(wrong_account.error == MessengerError::NotAMember); // not on the roster at all

        auto ok = accept_invite(store, /*account=*/2, *conv.conversation_id);
        assert(ok.error == MessengerError::None);
        assert(state_of(*store.get_conversation(*conv.conversation_id), 2) == MemberState::Active);

        // Idempotent: accepting again is a harmless no-op, not an error.
        auto again = accept_invite(store, 2, *conv.conversation_id);
        assert(again.error == MessengerError::None);
        std::cout << "accept_invite: Pending -> Active, wrong account rejected, idempotent\n";
    }

    // ---- leave_conversation: declines a Pending invite or leaves as Active ----
    {
        InMemoryConversationStore store;
        FakeSocialGraph graph;
        graph.set_friends(1, 2);
        auto conv = create_conversation(store, graph, 1, {2, 3}, true); // 2 Active, 3 Pending

        auto decline = leave_conversation(store, /*account=*/3, *conv.conversation_id);
        assert(decline.error == MessengerError::None);
        assert(!conversation_has_member(*store.get_conversation(*conv.conversation_id), 3));

        auto leave = leave_conversation(store, /*account=*/2, *conv.conversation_id);
        assert(leave.error == MessengerError::None);
        assert(!conversation_has_member(*store.get_conversation(*conv.conversation_id), 2));

        auto not_a_member = leave_conversation(store, /*account=*/99, *conv.conversation_id);
        assert(not_a_member.error == MessengerError::NotAMember);
        std::cout << "leave_conversation: declines a Pending invite, or leaves as Active, "
                     "same function either way\n";
    }

    std::cout << "\nall messenger tests passed\n";
    return 0;
}
