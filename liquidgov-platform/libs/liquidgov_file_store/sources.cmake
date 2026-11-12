# LGP1 store sources, in order (CMakeLists.txt here, deploy/rollback_guard.py and the checks that read the store's
# text all read this list; a new store module is added here and nowhere else). src/file_store.cpp is the frame: load()
# and flush() and the order of the record modules' read/finish/write calls. src/core/ is shared by every module;
# src/records/<module>.cpp holds one family of record types: its store methods, readers, finishers and writers.
set(LG_STORE_SOURCES
  src/file_store.cpp
  src/core/codec.cpp
  src/records/counters.cpp
  src/records/accounts.cpp
  src/records/social.cpp
  src/records/mail.cpp
  src/records/profile.cpp
  src/records/prefs.cpp
  src/records/pains.cpp
  src/records/posts.cpp
  src/records/proposals.cpp
  src/records/proposal_comments.cpp
  src/records/city.cpp
  src/records/communities.cpp
  src/records/notices.cpp
  src/records/admin.cpp
  src/records/avatars.cpp
  src/records/conversations.cpp
  src/records/jukebox.cpp
  src/records/styles.cpp
  src/records/here.cpp
)
