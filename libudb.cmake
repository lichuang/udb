set(libudb_files
  src/buffer/buffer_manager.cc
  src/storage/cursor.cc
  src/storage/mem_page.cc
  src/storage/txn_impl.cc
  src/storage/udb_impl.cc
)

add_library(udb 
  ${udb_SHARED_OR_STATIC}
  ${libudb_files}
)