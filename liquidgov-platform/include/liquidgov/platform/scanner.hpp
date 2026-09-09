#pragma once

#include <cstdint>
#include <vector>

namespace liquidgov::platform {

// Left abstract deliberately, same reasoning as BreachChecker in
// password.hpp: a real deployment plugs in a real scanning engine here.
// Not faked with an always-clean stub — an implementation that always
// returns true would be lying about being done, not a placeholder.
class MalwareScanner {
public:
    virtual ~MalwareScanner() = default;
    virtual bool is_clean(const std::vector<std::uint8_t>& bytes) const = 0;
};

} // namespace liquidgov::platform
