# liquidgov_store — LedgerFile / GovernanceStore / Replica persistence.
# Depends on: liquidgov_state (PUBLIC, declared in store.hpp).
add_library(liquidgov_store src/store.cpp)
target_include_directories(liquidgov_store PUBLIC include)
target_link_libraries(liquidgov_store PUBLIC liquidgov_state)
