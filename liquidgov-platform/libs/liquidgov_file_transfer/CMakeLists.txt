# liquidgov_file_transfer — default-deny ingest pipeline for attachments.
# Depends on liquidgov_blob, liquidgov_file_type, liquidgov_scanner (PUBLIC,
# file_transfer.hpp); liquidgov_image is PRIVATE (file_transfer.cpp only).
add_library(liquidgov_file_transfer src/file_transfer.cpp)
target_include_directories(liquidgov_file_transfer PUBLIC include)
target_link_libraries(liquidgov_file_transfer
    PUBLIC liquidgov_blob liquidgov_file_type liquidgov_scanner
    PRIVATE liquidgov_image
)
