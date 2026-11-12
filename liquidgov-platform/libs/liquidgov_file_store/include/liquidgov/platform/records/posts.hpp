#pragma once
// records/posts.hpp: the posts record family's types and helpers (moved out of file_store.hpp 2026-11-13, code
// moved, not rewritten). The LGP1 format of every record type stays in file_store.hpp's header comment.

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace liquidgov::platform {

// A post's own audience -- the /feed "Anybody"/"Friends"/"Groups" chips.
// Unlike Visibility, this isn't a ladder: Anybody and GroupsOnly are two
// distinct, non-nested reach shapes (public-minus-exceptions vs.
// community-membership-only), and Friends carries its own allow-list
// rather than a single yes/no. See the "pau" LGP1 tag's comment for the
// exact semantics of each value, including how "Friends + Groups" is
// represented (Friends here, plus Post::community/extra_groups set).
enum class PostAudience : int {
    Friends = 0,
    Anybody = 1,
    GroupsOnly = 2,
};

struct Post {
    std::uint64_t id = 0;
    std::string author;
    std::int64_t ts = 0;
    std::string text;
    std::string community; // empty = no community; else the first/primary slug
    // Which /feed chip chose this post's reach -- see PostAudience's own
    // comment. Friends is the default and the only value every post had
    // before this field existed.
    PostAudience audience = PostAudience::Friends;
    // Anybody hide-list: these usernames don't see the post even though
    // Anybody would otherwise show it to them. Only meaningful when
    // audience == Anybody.
    std::vector<std::string> exceptions;
    // Friends show-list: only these usernames (plus the author) see the post
    // in the personal/friends feed. Empty means every current friend --
    // today's only behavior, and still the default for a brand new post.
    // Only meaningful when audience == Friends.
    std::vector<std::string> friend_allowlist;
    // Extra communities this post also appears in, beyond `community`
    // itself (which stays the first/primary slug for every reader that only
    // ever looked at that one field). "Friends + Groups" together is
    // audience == Friends with community/extra_groups set -- there is no
    // separate combined value.
    std::vector<std::string> extra_groups;
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

}  // namespace liquidgov::platform
