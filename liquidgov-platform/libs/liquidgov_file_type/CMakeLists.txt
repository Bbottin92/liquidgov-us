# liquidgov_file_type — content-sniff for mail attachments (zip/OOXML via
# miniz, PRIVATE: only used inside file_type.cpp).
add_library(liquidgov_file_type src/file_type.cpp)
target_include_directories(liquidgov_file_type PUBLIC include)
target_link_libraries(liquidgov_file_type PRIVATE third_party_miniz)
