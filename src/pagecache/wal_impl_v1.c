#include <stdio.h>

#include "memory/memory.h"
#include "misc/atomic.h"
#include "misc/error.h"
#include "os/file.h"
#include "os/os.h"
#include "wal.h"

typedef struct wal_index_header_t wal_index_header_t;
typedef struct wal_checkpoint_t wal_checkpoint_t;
typedef struct wal_hash_location_t wal_hash_location_t;

/*
** Index numbers for various locking bytes.   WAL_NREADER is the number
** of available reader locks and should be at least 3.  The default
** is SQLITE_SHM_NLOCK==8 and  WAL_NREADER==5.
**
** Technically, the various VFSes are free to implement these locks however
** they see fit.  However, compatibility is encouraged so that VFSes can
** interoperate.  The standard implemention used on both unix and windows
** is for the index number to indicate a byte offset into the
** WalCkptInfo.aLock[] array in the wal-index header.  In other words, all
** locks are on the shm file.  The WALINDEX_LOCK_OFFSET constant (which
** should be 120) is the location in the shm file for the first locking
** byte.
*/

#define SQLITE_SHM_NLOCK 8

// 写锁在所有锁中的偏移量
#define WAL_WRITE_LOCK 0
// 除了写锁以外的其他所有锁
#define WAL_ALL_BUT_WRITE 1
// checkpoint锁在所有锁中的偏移量
#define WAL_CKPT_LOCK 1
// 恢复锁在所有锁中的偏移量
#define WAL_RECOVER_LOCK 2
// 输入读锁索引，返回对应读锁在所有锁中的偏移量，因为读锁从3开始，所以+3
#define WAL_READ_LOCK(I) (3 + (I))
// 读索引的数量 = 所有锁数量 - 读锁起始位置3
#define WAL_NREADER (SQLITE_SHM_NLOCK - 3)

/*
** The following object holds a copy of the wal-index header content.
**
** The actual header in the wal-index consists of two copies of this
** object followed by one instance of the WalCkptInfo object.
** For all versions of SQLite through 3.10.0 and probably beyond,
** the locking bytes (WalCkptInfo.aLock) start at offset 120 and
** the total header size is 136 bytes.
**
** The szPage value can be any power of 2 between 512 and 32768, inclusive.
** Or it can be 1 to represent a 65536-byte page.  The latter case was
** added in 3.7.1 when support for 64K pages was added.
*/
struct wal_index_header_t {
  uint32_t version;       /* Wal-index version */
  uint32_t unused;        /* Unused (padding) field */
  uint32_t txnCnt;        /* Counter incremented each transaction */
  bool isInit;            /* true when initialized */
  uint8_t bigEndCksum;    /* True if checksums in WAL are big-endian */
  int pageSize;           /* Database page size in bytes. */
  wal_frame_t maxFrame;   /* Index of last valid frame in the WAL */
  uint32_t pageNum;       /* Size of database in pages */
  uint32_t frameCksum[2]; /* Checksum of last frame in log */
  uint32_t salt[2];       /* Two salt values copied from WAL header */
  uint32_t ckSum[2];      /* Checksum over all prior fields */
};

/*
** A copy of the following object occurs in the wal-index immediately
** following the second copy of the WalIndexHdr.  This object stores
** information used by checkpoint.
**
** nBackfill is the number of frames in the WAL that have been written
** back into the database. (We call the act of moving content from WAL to
** database "backfilling".)  The nBackfill number is never greater than
** WalIndexHdr.mxFrame.  nBackfill can only be increased by threads
** holding the WAL_CKPT_LOCK lock (which includes a recovery thread).
** However, a WAL_WRITE_LOCK thread can move the value of nBackfill from
** mxFrame back to zero when the WAL is reset.
**
** nBackfillAttempted is the largest value of nBackfill that a checkpoint
** has attempted to achieve.  Normally nBackfill==nBackfillAtempted, however
** the nBackfillAttempted is set before any backfilling is done and the
** nBackfill is only set after all backfilling completes.  So if a checkpoint
** crashes, nBackfillAttempted might be larger than nBackfill.  The
** WalIndexHdr.mxFrame must never be less than nBackfillAttempted.
**
** The aLock[] field is a set of bytes used for locking.  These bytes should
** never be read or written.
**
** There is one entry in aReadMark[] for each reader lock.  If a reader
** holds read-lock K, then the value in aReadMark[K] is no greater than
** the mxFrame for that reader.  The value READMARK_NOT_USED (0xffffffff)
** for any aReadMark[] means that entry is unused.  aReadMark[0] is
** a special case; its value is never used and it exists as a place-holder
** to avoid having to offset aReadMark[] indexs by one.  Readers holding
** WAL_READ_LOCK(0) always ignore the entire WAL and read all content
** directly from the database.
**
** The value of aReadMark[K] may only be changed by a thread that
** is holding an exclusive lock on WAL_READ_LOCK(K).  Thus, the value of
** aReadMark[K] cannot changed while there is a reader is using that mark
** since the reader will be holding a shared lock on WAL_READ_LOCK(K).
**
** The checkpointer may only transfer frames from WAL to database where
** the frame numbers are less than or equal to every aReadMark[] that is
** in use (that is, every aReadMark[j] for which there is a corresponding
** WAL_READ_LOCK(j)).  New readers (usually) pick the aReadMark[] with the
** largest value and will increase an unused aReadMark[] to mxFrame if there
** is not already an aReadMark[] equal to mxFrame.  The exception to the
** previous sentence is when nBackfill equals mxFrame (meaning that everything
** in the WAL has been backfilled into the database) then new readers
** will choose aReadMark[0] which has value 0 and hence such reader will
** get all their all content directly from the database file and ignore
** the WAL.
**
** Writers normally append new frames to the end of the WAL.  However,
** if nBackfill equals mxFrame (meaning that all WAL content has been
** written back into the database) and if no readers are using the WAL
** (in other words, if there are no WAL_READ_LOCK(i) where i>0) then
** the writer will first "reset" the WAL back to the beginning and start
** writing new content beginning at frame 1.
**
** We assume that 32-bit loads are atomic and so no locks are needed in
** order to read from any aReadMark[] entries.
*/
struct wal_checkpoint_t {
  uint32_t backfillFrame;         /* Number of WAL frames backfilled into DB */
  uint32_t readMark[WAL_NREADER]; /* Reader marks */
  uint8_t lock[SQLITE_SHM_NLOCK]; /* Reserved space for locks */
  uint32_t backfillAttempted;     /* WAL frames perhaps written, or maybe not */
  uint32_t notUsed0;              /* Available for future enhancements */
};
#define READMARK_NOT_USED 0xffffffff

/* A block of WALINDEX_LOCK_RESERVED bytes beginning at
** WALINDEX_LOCK_OFFSET is reserved for locks. Since some systems
** only support mandatory file-locks, we do not read or write data
** from the region of the file on which locks are applied.
*/
#define WALINDEX_LOCK_OFFSET                                                   \
  (sizeof(wal_index_header_t) * 2 + offsetof(wal_checkpoint_t, lock))
#define WALINDEX_HEADER_SIZE                                                   \
  (sizeof(wal_index_header_t) * 2 + sizeof(wal_checkpoint_t))

/* Size of header before each frame in wal */
#define WAL_FRAME_HEADER_SIZE 24

/* Size of write ahead log header, including checksum. */
#define WAL_HEADER_SIZE 32

/*
** Define the parameters of the hash tables in the wal-index file. There
** is a hash-table following every HASHTABLE_NPAGE page numbers in the
** wal-index.
**
** Changing any of these constants will alter the wal-index format and
** create incompatibilities.
*/
#define HASHTABLE_NPAGE 4096                  /* Must be power of 2 */
#define HASHTABLE_HASH_1 383                  /* Should be prime */
#define HASHTABLE_NSLOT (HASHTABLE_NPAGE * 2) /* Must be a power of 2 */

/*
** The block of page numbers associated with the first hash-table in a
** wal-index is smaller than usual. This is so that there is a complete
** hash-table on each aligned 32KB page of the wal-index.
*/
#define HASHTABLE_NPAGE_ONE                                                    \
  (HASHTABLE_NPAGE - (WALINDEX_HEADER_SIZE / sizeof(uint32_t)))

/*
** Return the offset of frame in the write-ahead log file,
** assuming a database page size of pageSize bytes. The offset returned
** is to the start of the write-ahead log frame-header.
*/
#define walFrameOffset(frame, pageSize)                                        \
  (WAL_HEADERSIZE + ((frame)-1) * (int64_t)((pageSize) + WAL_FRAME_HEADER_SIZE))

/*
** Possible values for wal_impl_v1_t.readOnly
*/
#define WAL_RDWR 0   /* Normal read/write connection */
#define WAL_RDONLY 1 /* The WAL file is readonly */

/*
** Each page of the wal-index mapping contains a hash-table made up of
** an array of HASHTABLE_NSLOT elements of the following type.
*/
typedef uint16_t hash_slot_t;

/*
** An instance of the wal_hash_location_t object is used to describe the
** location of a page hash table in the wal-index.
** This becomes the return *value from __wal_hash_get().
*/
struct wal_hash_location_t {
  volatile hash_slot_t *hash; /* Start of the wal-index hash table */
  volatile page_no_t *pageNo; /* pageNo[1] is the page of first frame indexed */
  wal_frame_t zeroFrame; /* One less than the frame number of first indexed */
};

/*
** An open write-ahead log file is represented by an instance of the
** following object.
*/
typedef struct wal_impl_v1_t {
  os_t *os;                         /* The os object used to create dbFile */
  file_t *dbFile;                   /* File handle for the database file */
  file_t *walFile;                  /* File handle for WAL file */
  uint64_t maxWalSize;              /* Truncate WAL to this size upon reset */
  int walIndexSize;                 /* Size of array walIndexData */
  volatile uint32_t **walIndexData; /* Pointer to wal-index content in memory */
  int pageSize;                     /* Database page size */
  int16_t readLock;          /* Which read lock is being held.  -1 for none */
  bool checkpointLock;       /* True if holding a checkpoint lock */
  uint8_t readOnly;          /* WAL_RDWR, WAL_RDONLY */
  wal_index_header_t header; /* Wal-index header for current transaction */
  wal_frame_t minFrame;      /* Ignore wal frames before this one */

#ifdef UDB_DEBUG
  bool lockError; /* True if a locking error has occurred */
#endif
} wal_impl_v1_t;

/* Static wal impl methods function forward declarations */
udb_code_t __FindFrame_impl_v1(wal_impl_t *, page_no_t, wal_frame_t *);
udb_code_t __ReadFrame_impl_v1(wal_impl_t *, wal_frame_t, uint32_t bufferSize,
                               void *buffer);
udb_code_t __BeginReadTransaction_impl_v1(wal_impl_t *, bool *);
void __Destroy_v1(wal_impl_t *);

/* Static internal function forward declarations */
static void __initWalImplV1(wal_t *, wal_impl_v1_t *);
static int __walFrameHashIndex(wal_frame_t frame);
static udb_code_t __wal_hash_get(wal_impl_v1_t *, int, wal_hash_location_t *);
static int __wal_hash_index(page_no_t no);
static int __wal_next_hash(int);
static inline udb_code_t __wal_index_page(wal_impl_v1_t *, int,
                                          volatile uint32_t **);

static udb_code_t __wal_index_page_realloc(wal_impl_v1_t *, int,
                                           volatile uint32_t **);

static udb_code_t __walIndexReadHeader(wal_impl_v1_t *, bool *);
static udb_code_t __walTryBeginRead(wal_impl_v1_t *, bool *, bool, int);

/* Static wal impl methods function forward implementations */

udb_code_t __FindFrame_impl_v1(wal_impl_t *arg, page_no_t no,
                               wal_frame_t *retFrame) {
  wal_impl_v1_t *impl = (wal_impl_v1_t *)arg;
  wal_frame_t readFrame = 0; /* If !=0, WAL frame to return data from */
  wal_frame_t lastFrame =
      impl->header.maxFrame; /* Last page in WAL for this reader */
  int hash;                  /* Used to loop through N hash tables */
  int minHash;

  *retFrame = 0;

  /* This routine is only be called from within a read transaction. */
  assert(impl->readLock >= 0
#ifdef UDB_DEBUG
         || impl->lockError
#endif
  );

  /* If the "last page" field of the wal-index header snapshot is 0, then
  ** no data will be read from the wal under any circumstances. Return early
  ** in this case as an optimization.  Likewise, if impl->readLock==0,
  ** then the WAL is ignored by the reader so return early, as if the
  ** WAL were empty.
  */
  if (lastFrame == 0 || impl->readLock == 0) {
    return UDB_OK;
  }

  minHash = __walFrameHashIndex(impl->minFrame);
  for (hash = __walFrameHashIndex(lastFrame); hash >= minHash; hash--) {
    wal_hash_location_t location; /* Hash table location */
    int key;                      /* Hash slot index */
    int collideNum;               /* Number of hash collisions remaining */
    udb_code_t rc;                /* Error code */
    hash_slot_t hashSlot;

    rc = __wal_hash_get(impl, hash, &location);
    if (rc != UDB_OK) {
      return rc;
    }

    collideNum = HASHTABLE_NSLOT;
    key = __wal_hash_index(no);
    while ((hashSlot = AtomicLoad(&location.hash[key])) != 0) {
      wal_frame_t frame = hashSlot + location.zeroFrame;
      if (frame <= lastFrame && frame >= impl->minFrame &&
          location.pageNo[hashSlot] == no) {
        assert(frame > readFrame);
        readFrame = frame;
      }
      if ((collideNum--) == 0) {
        return UDB_CORRUPT_BKPT;
      }

      key = __wal_next_hash(key);
    }
    if (readFrame != 0) {
      break;
    }
  }

  *retFrame = readFrame;
  return UDB_OK;
}

udb_code_t __ReadFrame_impl_v1(wal_impl_t *arg, wal_frame_t readFrame,
                               uint32_t bufferSize, void *buffer) {
  int size;
  int64_t offset;
  wal_impl_v1_t *impl = (wal_impl_v1_t *)arg;

  size = impl->header.pageSize;
  size = (size & 0xfe00) + ((size & 0x0001) << 16);
  offset = walFrameOffset(readFrame, size) + WAL_FRAME_HEADER_SIZE;

  return os_read(impl->walFile, buffer, (bufferSize > size ? size : bufferSize),
                 offset);
}

udb_code_t __BeginReadTransaction_impl_v1(wal_impl_t *arg, bool *changed) {
  udb_code_t rc;
  int count = 0; /* Number of __walTryBeginRead attempts */
  bool retr wal_impl_v1_t *impl = (wal_impl_v1_t *)arg;

  assert(!impl->checkpointLock);

  do {
    rc = __walTryBeginRead(arg, changed, false, ++count);
  } while (rc == UDB_WAL_RETRY);

  return rc;
}

void __Destroy_v1(wal_impl_t *impl) {}

/* Static internal function implementations */

/* Outer function implementations */

/*
** Open a connection to the WAL file config->walName. The database file must
** already be opened on connection config->dbFile.
** The buffer that config->walName points
** to must remain valid for the lifetime of the returned wal_t* handle.
**
** A SHARED lock should be held on the database file when this function
** is called. The purpose of this SHARED lock is to prevent any other
** client from unlinking the WAL or wal-index file. If another process
** were to do this just after this client opened one of these files, the
** system would be badly broken.
**
** If the log file is successfully opened, UDB_OK is returned and
** *wal is set to point to a new WAL handle. If an error occurs,
** an UDB error code is returned and *wal is left unmodified.
*/
udb_code_t wal_open_impl_v1(wal_config_t *config, wal_t **wal) {
  udb_code_t ret = UDB_OK;
  wal_t *retWal = NULL;
  int flags;             /* Flags passed to OsOpen() */
  os_t *os = config->os; /* os module to open wal and wal-index */
  file_t *dbFile;        /* The open database file */
  const char *walName = config->walName; /* Name of the WAL file */
  wal_impl_v1_t *impl = NULL;

  assert(config->version == 1);
  assert(walName != NULL && walName[0] != '\0');
  assert(config->dbFile != NULL);

  *wal = NULL;

  retWal = (wal_t *)memory_calloc(sizeof(wal_t) + sizeof(wal_impl_v1_t) +
                                  os->sizeOfFile);
  if (retWal == NULL) {
    return UDB_OOM;
  }

  impl = (wal_impl_v1_t *)&retWal[1];
  impl->walFile = (file_t *)&impl[1];
  __initWalImplV1(retWal, impl);

  impl->os = os;
  impl->walFile = (file_t *)&impl[1];
  impl->dbFile = config->dbFile;
  impl->maxWalSize = config->maxWalSize;

  /* Open file handle on the write-ahead log file. */
  flags = (UDB_OPEN_READWRITE | UDB_OPEN_CREATE);
  ret = os_open(os, walName, impl->walFile, flags);
  if (ret != UDB_OK) {
    memory_free(retWal);
  }

  return ret;
}

/* Static internal function implementations */
static void __initWalImplV1(wal_t *wal, wal_impl_v1_t *impl) {
  wal->impl = impl;
  wal->version = 1;

  wal->methods = (wal_methods_t){
      __FindFrame_impl_v1, /* FindFrame */
      __ReadFrame_impl_v1, /* ReadFrame */
      __Destroy_v1,        /* Destroy */
  };
}

/*
** Return the number of the wal-index page that contains the hash-table
** and page-number array that contain entries corresponding to WAL frame
** iFrame. The wal-index is broken up into 32KB pages. Wal-index pages
** are numbered starting from 0.
*/
static int __walFrameHashIndex(wal_frame_t frame) {
  int hash =
      (frame + HASHTABLE_NPAGE - HASHTABLE_NPAGE_ONE - 1) / HASHTABLE_NPAGE;
  assert((hash == 0 || frame > HASHTABLE_NPAGE_ONE) &&
         (hash >= 1 || frame <= HASHTABLE_NPAGE_ONE) &&
         (hash <= 1 || frame > (HASHTABLE_NPAGE_ONE + HASHTABLE_NPAGE)) &&
         (hash >= 2 || frame <= HASHTABLE_NPAGE_ONE + HASHTABLE_NPAGE) &&
         (hash <= 2 || frame > (HASHTABLE_NPAGE_ONE + 2 * HASHTABLE_NPAGE)));
  assert(hash >= 0);
  return hash;
}

/*
** Return pointers to the hash table and page number array stored on
** page hash of the wal-index. The wal-index is broken into 32KB pages
** numbered starting from 0.
**
** Set output variable location->hash to point to the start of the hash table
** in the wal-index file. Set location->zeroFrame to one less than the frame
** number of the first frame indexed by this hash table. If a
** slot in the hash table is set to N, it refers to frame number
** (location->zeroFrame+N) in the log.
**
** Finally, set location->pageNo so that location->pageNo[1] is the page number
** of the first frame indexed by the hash table,
** frame (location->zeroFrame+1).
*/
static udb_code_t __wal_hash_get(wal_impl_v1_t *impl, int hash,
                                 wal_hash_location_t *location) {
  udb_code_t rc = UDB_OK;

  rc = __wal_index_page(impl, hash, &location->pageNo);
  assert(rc == UDB_OK || hash > 0);

  if (rc != UDB_OK) {
    return rc;
  }

  location->hash = (volatile hash_slot_t *)&location->pageNo[HASHTABLE_NPAGE];
  if (hash == 0) {
    location->pageNo =
        &location->pageNo[WALINDEX_HEADER_SIZE / sizeof(uint32_t)];
    location->zeroFrame = 0;
  } else {
    location->zeroFrame = HASHTABLE_NPAGE_ONE + (hash - 1) * HASHTABLE_NPAGE;
  }
  location->pageNo = &location->pageNo[-1];

  return rc;
}

/*
** Compute a hash on a page number.  The resulting hash value must land
** between 0 and (HASHTABLE_NSLOT-1).
** The __wal_next_hash() function advances the hash to the next value
** in the event of a collision.
*/
static int __wal_hash_index(page_no_t no) {
  assert(no > 0);
  assert((HASHTABLE_NSLOT & (HASHTABLE_NSLOT - 1)) == 0);
  return (no * HASHTABLE_HASH_1) & (HASHTABLE_NSLOT - 1);
}

static inline int __wal_next_hash(int priorHash) {
  return (priorHash + 1) & (HASHTABLE_NSLOT - 1);
}

static udb_code_t __wal_index_page(wal_impl_v1_t *impl, int hash,
                                   volatile uint32_t **pageNo) {
  if (impl->walIndexSize <= hash || (*pageNo = impl->walIndexData[hash]) == 0) {
    return __wal_index_page_realloc(impl, hash, pageNo);
  }
  return UDB_OK;
}

/*
** Obtain a pointer to the iPage'th page of the wal-index. The wal-index
** is broken into pages of WAL_INDEX_PAGE_SIZE bytes. Wal-index pages are
** numbered from zero.
**
** If the wal-index is currently smaller the iPage pages then the size
** of the wal-index might be increased, but only if it is safe to do
** so.  It is safe to enlarge the wal-index if impl->writeLock is true
** or impl->exclusiveMode==WAL_HEAPMEMORY_MODE.
**
** If this call is successful, *ppPage is set to point to the wal-index
** page and SQLITE_OK is returned. If an error (an OOM or VFS error) occurs,
** then an SQLite error code is returned and *ppPage is set to 0.
*/
static udb_code_t __wal_index_page_realloc(wal_impl_v1_t *impl, int hash,
                                           volatile uint32_t **pageNo) {
  udb_code_t rc = UDB_OK;

  *pageNo = NULL;
  /* Enlarge the impl->apWiData[] array if required */
  if (impl->walIndexSize <= hash) {
    uint32_t byte = sizeof(uint32_t) * (hash + 1);
    uint32_t **new = (uint32_t **)memory_realloc(impl->walIndexData, byte);
    if (new == NULL) {
      return;
    }
    memset((void *)&new[impl->walIndexSize], 0,
           sizeof(uint32_t *) * (hash + 1 - impl->walIndexSize));
    impl->walIndexData = new;
    impl->walIndexSize = hash + 1;
  }

  /* Request a pointer to the required page from the VFS */
  assert(impl->walIndexData[hash] == NULL);

  impl->walIndexData[hash] = (uint32_t *)memory_calloc(WAL_INDEX_PAGE_SIZE);
  if (impl->walIndexData[hash] == NULL) {
    rc = UDB_OOM;
  }

  *pageNo = impl->walIndexData[hash];
  assert(hash == 0 || *pageNo != NULL || rc != UDB_OK);
  return rc;
}

/*
** Attempt to start a read transaction.  This might fail due to a race or
** other transient condition.  When that happens, it returns UDB_WAL_RETRY to
** indicate to the caller that it is safe to retry immediately.
**
** On success return UDB_OK.  On a permanent failure (such an
** I/O error or an UDB_BUSY because another process is running
** recovery) return a positive error code.
**
** The useWal parameter is true to force the use of the WAL and disable
** the case where the WAL is bypassed because it has been completely
** checkpointed.  If useWal==false then this routine calls
** __walIndexReadHeader() to make a copy of the wal-index header into
** impl->header.  If the wal-index header has changed, *changed is set to true
** (as an indication to the caller that the local page cache is obsolete and
** needs to be flushed.)  When useWal==true, the wal-index header is assumed to
** already be loaded and the changed parameter is unused.
**
** The caller must set the counter parameter to the number of prior calls to
** this routine during the current read attempt that returned UDB_WAL_RETRY.
** This routine will start taking more aggressive measures to clear the
** race conditions after multiple UDB_WAL_RETRY returns, and after an excessive
** number of errors will ultimately return UDB_PROTOCOL.  The
** UDB_PROTOCOL return indicates that some other process has gone rogue
** and is not honoring the locking protocol.  There is a vanishingly small
** chance that UDB_PROTOCOL could be returned because of a run of really
** bad luck when there is lots of contention for the wal-index, but that
** possibility is so small that it can be safely neglected, we believe.
**
** On success, this routine obtains a read lock on
** WAL_READ_LOCK(impl->readLock).  The impl->readLock integer is
** in the range 0 <= impl->readLock < WAL_NREADER.  If impl->readLock==(-1)
** that means the Wal does not hold any read lock.  The reader must not
** access any database page that is modified by a WAL frame up to and
** including frame number aReadMark[impl->readLock].  The reader will
** use WAL frames up to and including impl->hdr.mxFrame if impl->readLock>0
** Or if impl->readLock==0, then the reader will ignore the WAL
** completely and get all content directly from the database file.
** If the useWal parameter is 1 then the WAL will never be ignored and
** this routine will always set impl->readLock>0 on success.
** When the read transaction is completed, the caller must release the
** lock on WAL_READ_LOCK(impl->readLock) and set impl->readLock to -1.
**
** This routine uses the nBackfill and aReadMark[] fields of the header
** to select a particular WAL_READ_LOCK() that strives to let the
** checkpoint process do as much work as possible.  This routine might
** update values of the aReadMark[] array in the header, but if it does
** so it takes care to hold an exclusive lock on the corresponding
** WAL_READ_LOCK() while changing values.
*/
static udb_code_t __walTryBeginRead(wal_impl_v1_t *impl, bool *changed,
                                    bool useWal, int count) {
  udb_code_t rc = UDB_OK;

  /* Not currently locked */
  assert(impl->readLock < 0);

  /* Take steps to avoid spinning forever if there is a protocol error.
  **
  ** Circumstances that cause a RETRY should only last for the briefest
  ** instances of time.  No I/O or other system calls are done while the
  ** locks are held, so the locks should not be held for very long. But
  ** if we are unlucky, another process that is holding a lock might get
  ** paged out or take a page-fault that is time-consuming to resolve,
  ** during the few nanoseconds that it is holding the lock.  In that case,
  ** it might take longer than normal for the lock to free.
  **
  ** After 5 RETRYs, we begin calling os_sleep().  The first few
  ** calls to os_sleep() have a delay of 1 microsecond.  Really this
  ** is more of a scheduler yield than an actual delay.  But on the 10th
  ** an subsequent retries, the delays start becoming longer and longer,
  ** so that on the 100th (and last) RETRY we delay for 323 milliseconds.
  ** The total delay time before giving up is less than 10 seconds.
  */
  if (count > 5) {
    int delayMicroSec = 1; /* Pause time in microseconds */
    if (count > 100) {
      return UDB_PROTOCOL;
    }
    if (count >= 10) {
      delayMicroSec = (count - 9) * (count - 9) * 39;
    }
    os_sleep(impl->os, delayMicroSec);
  }
  return rc;
}