# liquidgov_events — hashable event-sourced ledger records.
# Depends on: liquidgov_hash, liquidgov_ids (both PUBLIC — declared in events.hpp).
add_library(liquidgov_events src/events.cpp)
target_include_directories(liquidgov_events PUBLIC include)
target_link_libraries(liquidgov_events PUBLIC liquidgov_hash liquidgov_ids)
