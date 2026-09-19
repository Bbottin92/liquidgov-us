#pragma once

namespace commons {

// The commit this binary was built from: 7-40 lowercase hex, optionally "+dirty", or "unknown". Generated at build
// time by cmake/gen_build_info.cmake (see there for where it comes from); shown by GET /health.
const char* build_rev();

} // namespace commons
