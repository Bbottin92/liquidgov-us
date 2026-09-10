cmake_minimum_required(VERSION 3.16)
project(liquidgov-core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(libs/liquidgov_hash)
add_subdirectory(libs/liquidgov_ids)
add_subdirectory(libs/liquidgov_law)
add_subdirectory(libs/liquidgov_events)
add_subdirectory(libs/liquidgov_state)
add_subdirectory(libs/liquidgov_store)

foreach(t IN ITEMS smoke persist delegation replica identity land judgment)
    add_executable(liquidgov_${t} tests/${t}.cpp)
    target_link_libraries(liquidgov_${t} PRIVATE liquidgov_store)
endforeach()
