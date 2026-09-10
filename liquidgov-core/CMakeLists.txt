cmake_minimum_required(VERSION 3.16)
project(liquidgov-core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(liquidgov_core
    src/law.cpp
    src/hash.cpp
    src/events.cpp
    src/state.cpp
    src/store.cpp
)
target_include_directories(liquidgov_core PUBLIC include)

add_executable(liquidgov_smoke tests/smoke.cpp)
target_link_libraries(liquidgov_smoke PRIVATE liquidgov_core)

add_executable(liquidgov_persist tests/persist.cpp)
target_link_libraries(liquidgov_persist PRIVATE liquidgov_core)

add_executable(liquidgov_delegation tests/delegation.cpp)
target_link_libraries(liquidgov_delegation PRIVATE liquidgov_core)

add_executable(liquidgov_replica tests/replica.cpp)
target_link_libraries(liquidgov_replica PRIVATE liquidgov_core)

add_executable(liquidgov_identity tests/identity.cpp)
target_link_libraries(liquidgov_identity PRIVATE liquidgov_core)

add_executable(liquidgov_land tests/land.cpp)
target_link_libraries(liquidgov_land PRIVATE liquidgov_core)

add_executable(liquidgov_judgment tests/judgment.cpp)
target_link_libraries(liquidgov_judgment PRIVATE liquidgov_core)
