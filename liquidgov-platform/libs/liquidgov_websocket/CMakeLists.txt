# liquidgov_websocket — RFC 6455 handshake + frame read/write over a raw
# POSIX socket fd. No dependency on the rest of liquidgov-platform; genuinely
# reusable in any project that needs bare-bones WebSocket support without
# pulling in a full networking library.
add_library(liquidgov_websocket src/websocket.cpp)
target_include_directories(liquidgov_websocket PUBLIC include)
