#include "buildinfo.hpp"

// The ONLY file that includes the generated header, so a changed revision recompiles just this one (the droplet is a
// 512MB box building with -j1).
#include "build_info.h"

namespace commons {

const char* build_rev() { return COMMONS_BUILD_REV; }

} // namespace commons
