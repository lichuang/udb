set(libudb_files
  src/misc/error.c
  src/global.c
  src/page.c
  src/pagecache/pager.c
  src/pagecache/page_cache.c
  src/pagecache/default_cache_methods.c
)

add_library(udb 
  ${udb_SHARED_OR_STATIC}
  ${libudb_files}
)