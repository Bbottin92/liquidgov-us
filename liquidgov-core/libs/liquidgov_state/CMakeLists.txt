# liquidgov_state — Polity domain state (no I/O).
# Depends on: liquidgov_events (PUBLIC, declared in state.hpp),
#             liquidgov_law (PRIVATE, only used inside state.cpp).
add_library(liquidgov_state src/state.cpp)
target_include_directories(liquidgov_state PUBLIC include)
target_link_libraries(liquidgov_state PUBLIC liquidgov_events PRIVATE liquidgov_law)
