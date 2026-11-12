# LGP1 store sources, in order (CMakeLists.txt here, deploy/rollback_guard.py and the checks that read the store's
# text all read this list; a new store module is added here and nowhere else).
set(LG_STORE_SOURCES
  src/file_store.cpp
)
