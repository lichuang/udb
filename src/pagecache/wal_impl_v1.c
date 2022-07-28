#include <stdio.h>

#include "memory/memory.h"
#include "misc/atomic.h"
#include "misc/error.h"
#include "os/file.h"
#include "os/os.h"
#include "wal.h"

/**
** This file contains the implementation of a write-ahead log (WAL).
**
** WRITE-AHEAD LOG (WAL) FILE FORMAT
**
** A WAL file consists of a header followed by zero or more "frames".
** Each frame records the revised content of a single page from the
** database file.  All changes to the database are recorded by writing
** frames into the WAL.  Transactions commit when a frame is written that
** contains a commit marker.  A single WAL can and usually does record
** multiple transactions.  Periodically, the content of the WAL is
** transferred back into the database file in an operation called a
** "checkpoint".
**
** A single WAL file can be used multiple times.  In other words, the
** WAL can fill up with frames and then be checkpointed and then new
** frames can overwrite the old ones.  A WAL always grows from beginning
** toward the end.  Checksums and counters attached to each frame are
** used to determine which frames within the WAL are valid and which
** are leftovers from prior checkpoints.
**
** The WAL header is 32 bytes in size and consists of the following eight
** big-endian 32-bit unsigned integer values:
**
**     0: Magic number.  0x377f0682 or 0x377f0683
**     4: File format version.  Currently 3007000
**     8: Database page size.  Example: 1024
**    12: Checkpoint sequence number
**    16: Salt-1, random integer incremented with each checkpoint
**    20: Salt-2, a different random integer changing with each ckpt
**    24: Checksum-1 (first part of checksum for first 24 bytes of header).
**    28: Checksum-2 (second part of checksum for first 24 bytes of header).
**
** Immediately following the wal-header are zero or more frames. Each
** frame consists of a 24-byte frame-header followed by a <page-size> bytes
** of page data. The frame-header is six big-endian 32-bit unsigned
** integer values, as follows:
**
**     0: Page number.
**     4: For commit records, the size of the database image in pages
**        after the commit. For all other records, zero.
**     8: Salt-1 (copied from the header)
**    12: Salt-2 (copied from the header)
**    16: Checksum-1.
**    20: Checksum-2.
**
** A frame is considered valid if and only if the following conditions are
** true:
**
**    (1) The salt-1 and salt-2 values in the frame-header match
**        salt values in the wal-header
**
**    (2) The checksum values in the final 8 bytes of the frame-header
**        exactly match the checksum computed consecutively on the
**        WAL header and the first 8 bytes and the content of all frames
**        up to and including the current frame.
**
** The checksum is computed using 32-bit big-endian integers if the
** magic number in the first 4 bytes of the WAL is 0x377f0683 and it
** is computed using little-endian if the magic number is 0x377f0682.
** The checksum values are always stored in the frame header in a
** big-endian format regardless of which byte order is used to compute
** the checksum.  The checksum is computed by interpreting the input as
** an even number of unsigned 32-bit integers: x[0] through x[N].  The
** algorithm used for the checksum is as follows:
**
**   for i from 0 to n-1 step 2:
**     s0 += x[i] + s1;
**     s1 += x[i+1] + s0;
**   endfor
**
** Note that s0 and s1 are both weighted checksums using fibonacci weights
** in reverse order (the largest fibonacci weight occurs on the first element
** of the sequence being summed.)  The s1 value spans all 32-bit
** terms of the sequence whereas s0 omits the final term.
**
** On a checkpoint, the WAL is first VFS.xSync-ed, then valid content of the
** WAL is transferred into the database, then the database is VFS.xSync-ed.
** The VFS.xSync operations serve as write barriers - all writes launched
** before the xSync must complete before any write that launches after the
** xSync begins.
**
** After each checkpoint, the salt-1 value is incremented and the salt-2
** value is randomized.  This prevents old and new frames in the WAL from
** being considered valid at the same time and being checkpointing together
** following a crash.
**
** READER ALGORITHM
**
** To read a page from the database (call it page number P), a reader
** first checks the WAL to see if it contains page P.  If so, then the
** last valid instance of page P that is a followed by a commit frame
** or is a commit frame itself becomes the value read.  If the WAL
** contains no copies of page P that are valid and which are a commit
** frame or are followed by a commit frame, then page P is read from
** the database file.
**
** To start a read transaction, the reader records the index of the last
** valid frame in the WAL.  The reader uses this recorded "mxFrame" value
** for all subsequent read operations.  New transactions can be appended
** to the WAL, but as long as the reader uses its original mxFrame value
** and ignores the newly appended content, it will see a consistent snapshot
** of the database from a single point in time.  This technique allows
** multiple concurrent readers to view different versions of the database
** content simultaneously.
**
** The reader algorithm in the previous paragraphs works correctly, but
** because frames for page P can appear anywhere within the WAL, the
** reader has to scan the entire WAL looking for page P frames.  If the
** WAL is large (multiple megabytes is typical) that scan can be slow,
** and read performance suffers.  To overcome this problem, a separate
** data structure called the wal-index is maintained to expedite the
** search for frames of a particular page.
**
** WAL-INDEX FORMAT
**
** Conceptually, the wal-index is shared memory, though VFS implementations
** might choose to implement the wal-index using a mmapped file.  Because
** the wal-index is shared memory, SQLite does not support journal_mode=WAL
** on a network filesystem.  All users of the database must be able to
** share memory.
**
** In the default unix and windows implementation, the wal-index is a mmapped
** file whose name is the database name with a "-shm" suffix added.  For that
** reason, the wal-index is sometimes called the "shm" file.
**
** The wal-index is transient.  After a crash, the wal-index can (and should
** be) reconstructed from the original WAL file.  In fact, the VFS is required
** to either truncate or zero the header of the wal-index when the last
** connection to it closes.  Because the wal-index is transient, it can
** use an architecture-specific format; it does not have to be cross-platform.
** Hence, unlike the database and WAL file formats which store all values
** as big endian, the wal-index can store multi-byte values in the native
** byte order of the host computer.
**
** The purpose of the wal-index is to answer this question quickly:  Given
** a page number P and a maximum frame index M, return the index of the
** last frame in the wal before frame M for page P in the WAL, or return
** NULL if there are no frames for page P in the WAL prior to M.
**
** The wal-index consists of a header region, followed by an one or
** more index blocks.
**
** The wal-index header contains the total number of frames within the WAL
** in the mxFrame field.
**
** Each index block except for the first contains information on
** HASHTABLE_NPAGE frames. The first index block contains information on
** HASHTABLE_NPAGE_ONE frames. The values of HASHTABLE_NPAGE_ONE and
** HASHTABLE_NPAGE are selected so that together the wal-index header and
** first index block are the same size as all other index blocks in the
** wal-index.
**
** Each index block contains two sections, a page-mapping that contains the
** database page number associated with each wal frame, and a hash-table
** that allows readers to query an index block for a specific page number.
** The page-mapping is an array of HASHTABLE_NPAGE (or HASHTABLE_NPAGE_ONE
** for the first index block) 32-bit page numbers. The first entry in the
** first index-block contains the database page number corresponding to the
** first frame in the WAL file. The first entry in the second index block
** in the WAL file corresponds to the (HASHTABLE_NPAGE_ONE+1)th frame in
** the log, and so on.
**
** The last index block in a wal-index usually contains less than the full
** complement of HASHTABLE_NPAGE (or HASHTABLE_NPAGE_ONE) page-numbers,
** depending on the contents of the WAL file. This does not change the
** allocated size of the page-mapping array - the page-mapping array merely
** contains unused entries.
**
** Even without using the hash table, the last frame for page P
** can be found by scanning the page-mapping sections of each index block
** starting with the last index block and moving toward the first, and
** within each index block, starting at the end and moving toward the
** beginning.  The first entry that equals P corresponds to the frame
** holding the content for that page.
**
** The hash table consists of HASHTABLE_NSLOT 16-bit unsigned integers.
** HASHTABLE_NSLOT = 2*HASHTABLE_NPAGE, and there is one entry in the
** hash table for each page number in the mapping section, so the hash
** table is never more than half full.  The expected number of collisions
** prior to finding a match is 1.  Each entry of the hash table is an
** 1-based index of an entry in the mapping section of the same
** index block.   Let K be the 1-based index of the largest entry in
** the mapping section.  (For index blocks other than the last, K will
** always be exactly HASHTABLE_NPAGE (4096) and for the last index block
** K will be (mxFrame%HASHTABLE_NPAGE).)  Unused slots of the hash table
** contain a value of 0.
**
** To look for page P in the hash table, first compute a hash iKey on
** P as follows:
**
**      iKey = (P * 383) % HASHTABLE_NSLOT
**
** Then start scanning entries of the hash table, starting with iKey
** (wrapping around to the beginning when the end of the hash table is
** reached) until an unused hash slot is found. Let the first unused slot
** be at index iUnused.  (iUnused might be less than iKey if there was
** wrap-around.) Because the hash table is never more than half full,
** the search is guaranteed to eventually hit an unused entry.  Let
** iMax be the value between iKey and iUnused, closest to iUnused,
** where aHash[iMax]==P.  If there is no iMax entry (if there exists
** no hash slot such that aHash[i]==p) then page P is not in the
** current index block.  Otherwise the iMax-th mapping entry of the
** current index block corresponds to the last entry that references
** page P.
**
** A hash search begins with the last index block and moves toward the
** first index block, looking for entries corresponding to page P.  On
** average, only two or three slots in each index block need to be
** examined in order to either find the last entry for page P, or to
** establish that no such entry exists in the block.  Each index block
** holds over 4000 entries.  So two or three index blocks are sufficient
** to cover a typical 10 megabyte WAL file, assuming 1K pages.  8 or 10
** comparisons (on average) suffice to either locate a frame in the
** WAL or to establish that the frame does not exist in the WAL.  This
** is much faster than scanning the entire 10MB WAL.
**
** Note that entries are added in order of increasing K.  Hence, one
** reader might be using some value K0 and a second reader that started
** at a later time (after additional transactions were added to the WAL
** and to the wal-index) might be using a different value K1, where K1>K0.
** Both readers can use the same hash table and mapping section to get
** the correct result.  There may be entries in the hash table with
** K>K0 but to the first reader, those entries will appear to be unused
** slots in the hash table and so the first reader will get an answer as
** if no values greater than K0 had ever been inserted into the hash table
** in the first place - which is what reader one wants.  Meanwhile, the
** second reader using K1 will see additional values that were inserted
** later, which is exactly what reader two wants.
**
** When a rollback occurs, the value of K is decreased. Hash table entries
** that correspond to frames greater than the new K value are removed
** from the hash table at this point.
*/

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

static inline bool __walIsIndexHeaderChanged(wal_impl_v1_t *);
static udb_code_t __walIndexReadHeader(wal_impl_v1_t *, bool *);
static udb_code_t __walTryBeginRead(wal_impl_v1_t *, bool *, bool, int);
static udb_code_t __walTryReadHeader(wal_impl_v1_t *, bool *);
static volatile wal_index_header_t *__walIndexHeader(wal_impl_v1_t *);

static volatile wal_checkpoint_t *__walCheckpoint(wal_impl_v1_t *);

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

static inline bool __walIsIndexHeaderChanged(wal_impl_v1_t *impl) {
  return memcmp((void *)__walIndexHeader(impl), &impl->header,
                sizeof(wal_index_header_t)) != 0;
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
  volatile wal_checkpoint_t *checkpoint = NULL;
  uint32_t maxReadMark; /* Largest readMark[] value */
  int maxIndex;         /* Index of largest readMark[] value */
  wal_frame_t maxFrame; /* Wal frame to lock to */
  int i;                /* Loop counter */

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

  if (!useWal) {
    return __walTryReadHeader(impl, changed);
  }

  assert(impl->walIndexSize > 0);
  assert(impl->walIndexData[0] != NULL);

  checkpoint = __walCheckpoint(impl);
  if (!useWal &&
      AtomicLoad(&checkpoint->backfillFrame) == impl->header.maxFrame) {
    /* The WAL has been completely backfilled (or it is empty).
    ** and can be safely ignored.
    */
    if (__walIsIndexHeaderChanged(impl)) {
      /* It is not safe to allow the reader to continue here if frames
      ** may have been appended to the log before READ_LOCK(0) was obtained.
      ** When holding READ_LOCK(0), the reader ignores the entire log file,
      ** which implies that the database file contains a trustworthy
      ** snapshot. Since holding READ_LOCK(0) prevents a checkpoint from
      ** happening, this is usually correct.
      **
      ** However, if frames have been appended to the log (or if the log
      ** is wrapped and written for that matter) before the READ_LOCK(0)
      ** is obtained, that is not necessarily true. A checkpointer may
      ** have started to backfill the appended frames but crashed before
      ** it finished. Leaving a corrupt image in the database file.
      */
      return UDB_WAL_RETRY;
    }
    impl->readLock = 0;
    return UDB_OK;
  }

  /* If we get this far, it means that the reader will want to use
  ** the WAL to get at content from recent commits.  The job now is
  ** to select one of the readMark[] entries that is closest to
  ** but not exceeding impl->header.maxFrame and lock that entry.
  */
  maxReadMark = 0;
  maxIndex = 0;
  maxFrame = impl->header.maxFrame;
  for (i = 1; i < WAL_NREADER; i++) {
    uint32_t thisMark = AtomicLoad(checkpoint->readMark + i);
    if (maxReadMark <= thisMark && thisMark <= maxFrame) {
      assert(thisMark != READMARK_NOT_USED);
      maxReadMark = thisMark;
      maxIndex = i;
    }
  }

  if (maxReadMark < maxFrame || maxIndex == 0) {
    for (i = 1; i < WAL_NREADER; i++) {
      AtomicStore(checkpoint->readMark + i, maxFrame);
      maxReadMark = maxFrame;
      maxIndex = i;
      break;
    }
  }
  if (maxIndex == 0) {
    return UDB_WAL_RETRY;
  }

  /* Now that the read-lock has been obtained, check that neither the
  ** value in the aReadMark[] array or the contents of the wal-index
  ** header have changed.
  **
  ** It is necessary to check that the wal-index header did not change
  ** between the time it was read and when the shared-lock was obtained
  ** on WAL_READ_LOCK(mxI) was obtained to account for the possibility
  ** that the log file may have been wrapped by a writer, or that frames
  ** that occur later in the log than pWal->hdr.mxFrame may have been
  ** copied into the database by a checkpointer. If either of these things
  ** happened, then reading the database with the current value of
  ** pWal->hdr.mxFrame risks reading a corrupted snapshot. So, retry
  ** instead.
  **
  ** Before checking that the live wal-index header has not changed
  ** since it was read, set Wal.minFrame to the first frame in the wal
  ** file that has not yet been checkpointed. This client will not need
  ** to read any frames earlier than minFrame from the wal file - they
  ** can be safely read directly from the database file.
  **
  ** Because a ShmBarrier() call is made between taking the copy of
  ** nBackfill and checking that the wal-header in shared-memory still
  ** matches the one cached in pWal->hdr, it is guaranteed that the
  ** checkpointer that set nBackfill was not working with a wal-index
  ** header newer than that cached in pWal->hdr. If it were, that could
  ** cause a problem. The checkpointer could omit to checkpoint
  ** a version of page X that lies before pWal->minFrame (call that version
  ** A) on the basis that there is a newer version (version B) of the same
  ** page later in the wal file. But if version B happens to like past
  ** frame pWal->hdr.mxFrame - then the client would incorrectly assume
  ** that it can read version A from the database file. However, since
  ** we can guarantee that the checkpointer that set nBackfill could not
  ** see any pages past pWal->hdr.mxFrame, this problem does not come up.
  */
  impl->minFrame = AtomicLoad(&checkpoint->backfillFrame) + 1;

  if (AtomicLoad(&checkpoint->readMark + maxIndex) != maxReadMark ||
      __walIsIndexHeaderChanged(impl)) {
    return UDB_WAL_RETRY;
  } else {
    assert(maxReadMark <= impl->header.maxFrame);
    impl->readLock = (int16_t)maxIndex;
  }

  return rc;
}

static udb_code_t __walTryReadHeader(wal_impl_v1_t *impl, bool *changed) {}