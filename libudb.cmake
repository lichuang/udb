set(libudb_files
  src/page.c
  src/pager.c
  src/page_cache.c
  src/default_cache_methods.c
)

add_library(udb 
  ${udb_SHARED_OR_STATIC}
  ${libudb_files}
)