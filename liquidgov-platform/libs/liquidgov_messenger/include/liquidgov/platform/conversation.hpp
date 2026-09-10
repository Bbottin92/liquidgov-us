#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "liquidgov/platform/account.hpp"

namespace liquidgov::platform {

using ConversationId = std::uint64_t;
using MessageId = std::uint64_t;

enum class MemberState { Active, Pending };

struct ConversationMember {
    AccountId account = 0;
    MemberState state = MemberState::Active;
};

struct Conversation {
    ConversationId id = 0;
    std::vector<ConversationMember> members;
    bool is_group = false;
    std::int64_t created_at = 0;
};

enum class DeletionState { NotDeleted, DeletedForSender, DeletedForEveryone };

struct Message {
    MessageId id = 0;
    ConversationId conversation_id = 0;
    AccountId sender = 0;
    std::int64_t sent_at = 0;
    std::string text;

    // Edits are never silent — prior versions are kept, not overwritten.
    std::vector<std::string> edit_history;
    std::int64_t edited_at = 0; // 0 = never edited

    DeletionState deletion = DeletionState::NotDeleted;
    std::int64_t deleted_at = 0;

    // Set by report_message(). Exempts this message from the retention-
    // window purge regardless of deleted_at — a takedown or a deletion
    // must not be able to vacuum evidence out from under an open report.
    bool under_review = false;
};

} // namespace liquidgov::platform
