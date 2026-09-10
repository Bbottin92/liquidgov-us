# liquidgov_messenger — group conversation business rules. Bundles
# conversation.hpp + social_graph.hpp (header-only interfaces with no reuse
# outside messaging). PUBLIC liquidgov_account (both bundled headers need
# it); liquidgov_clock is PRIVATE (messenger.cpp only).
add_library(liquidgov_messenger src/messenger.cpp)
target_include_directories(liquidgov_messenger PUBLIC include)
target_link_libraries(liquidgov_messenger
    PUBLIC liquidgov_account
    PRIVATE liquidgov_clock
)
