#!/usr/bin/env bash
# Run inside Ubuntu WSL from the liquidgov-platform directory.
set -euo pipefail
sudo apt-get update
sudo apt-get install -y g++ cmake pkg-config libssl-dev libsodium-dev
OUT="${1:-$HOME/liquidgov_http}"
g++ -O2 -std=c++17 \
  -I include -I third_party/stb -I third_party/miniz \
  src/http_server.cpp src/auth.cpp src/password.cpp src/session.cpp src/file_store.cpp \
  src/blob.cpp src/avatar.cpp src/image_normalize.cpp \
  src/file_type.cpp src/file_transfer.cpp \
  third_party/miniz/miniz.c third_party/miniz/miniz_tdef.c \
  third_party/miniz/miniz_tinfl.c third_party/miniz/miniz_zip.c \
  -lsodium -pthread \
  -o "$OUT"
echo
echo "Built $OUT"
echo "Store: \$LIQUIDGOV_STORE (default ./liquidgov_accounts.lgp)"
echo "Blobs: \$LIQUIDGOV_BLOBS (default ./liquidgov_blobs)"
echo "Run:  $OUT"
echo "Open: http://127.0.0.1:8080"
echo "Binary liquidgov_http, port 8080, bind 0.0.0.0. Reverse proxy is not part of this binary."
