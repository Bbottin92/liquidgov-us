# liquidgov_auth — create_account / login / logout. Depends on
# liquidgov_account, liquidgov_password, liquidgov_session (PUBLIC, all
# declared in auth.hpp); liquidgov_clock is PRIVATE (auth.cpp only).
add_library(liquidgov_auth src/auth.cpp)
target_include_directories(liquidgov_auth PUBLIC include)
target_link_libraries(liquidgov_auth
    PUBLIC liquidgov_account liquidgov_password liquidgov_session
    PRIVATE liquidgov_clock
)
