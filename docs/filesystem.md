# Filesystem

Arblem includes a 4 KiB LRU block cache (`kernel/bcache.c`), a simple UNIX-like
filesystem (`kernel/kfs.c`, `inc/fs.h`), and a host tool (`tools/mkfs.c`) that
builds the initial disk image.

## Block cache (`kernel/bcache.c`)

Each filesystem block is 4 KiB (`BCACHE_BLOCK_SIZE = 4096`), matching `PAGE_SIZE`
and spanning 8 hardware sectors of 512 bytes (`VirtIO-blk`).

- **Buffer pool**: `bcache_bufs[BCACHE_COUNT]` (16 buffers, 64 KiB total) sits
  in identity-mapped `.bss`, so each buffer's `data` array can be passed
  directly to VirtIO DMA.
- **LRU list and locking**: `bcache_lock` (`struct spinlock`) guards the
  circular doubly-linked list (`bcache_head`) and buffer metadata (`ref`,
  `busy`, `valid`). `bcache_get(blockno)` checks for a match first; on a hit it
  increments `ref` and returns the pinned buffer.
- **Busy synchronization**: On a cache miss, `bcache_get` recycles the least
  recently used unpinned buffer (`ref == 0 && !busy`), sets `busy = 1` and
  `ref = 1`, releases `bcache_lock` during `bcache_read_block` (because
  `virtio_blk_read` sleeps on DMA completion), and re-acquires `bcache_lock`
  afterward. Other threads looking up the same `blockno` sleep with
  `thread_sleep(&bcache_wait, &bcache_lock)` until the read finishes, preventing
  duplicate cache entries.
- **Write-through and release**: `bcache_write(b)` writes all 8 sectors back to
  disk immediately. `bcache_put(b)` decrements `ref` and, when it reaches zero,
  moves the buffer to the MRU end (`bcache_head.next`).

## On-disk layout (`inc/fs.h`, `tools/mkfs.c`)

`tools/mkfs.c` creates a 2 MiB image (512 blocks of 4 KiB):

| Blocks     | Contents                                                    |
|------------|-------------------------------------------------------------|
| `0`        | Reserved / boot block (untouched by the FS; `blkcheck` tests raw sectors here) |
| `1`        | Superblock (`struct fs_super`, magic `FS_MAGIC = 0x41524246` `"ARBF"`) |
| `2`-`3`    | Inode table (`128` 64-byte `struct fs_dinode` entries; inode 0 unused, inode 1 is `/`) |
| `4`        | Allocation bitmap (1 bit per block)                         |
| `5`-`511`  | Data blocks: file contents, directories, indirect blocks    |

Each `struct fs_dinode` (64 bytes) holds `type` (`FS_TYPE_FREE`, `FS_TYPE_FILE`,
`FS_TYPE_DIR`), `nlink`, `size`, 12 direct block pointers (`FS_NDIRECT`, 48 KiB),
and 1 single indirect block pointer (`FS_NINDIRECT` = 1024 entries, allowing files
up to 1036 blocks ≈ 4 MiB on larger disks). Directories store sequences of
32-byte `struct fs_dirent` entries (`inum` + 28-byte NUL-padded `name`).

During the build, `build/mkfs` populates `/` with `.`, `..`, and every user ELF
in `USER_PROGS` with its `.elf` suffix stripped.

## Kernel filesystem (`kernel/kfs.c`)

`kernel_init` calls `fs_init()` once the scheduler and all harts are running so
disk reads can sleep on VirtIO interrupts. `fs_init()` reads and validates
block 1.

- **Locking (`fs_lock`)**: Public filesystem operations and `fs_files[]`
  (`fs_open`, `fs_read`, `fs_write`, `fs_create`, `fs_unlink`, `fs_stat`,
  `fs_trunc`, `fs_lookup`, `fs_file_read`, `fs_file_write`, `fs_file_close`)
  acquire `fs_lock` (`struct sleeplock` in `kernel/sleeplock.c`), allowing
  disk I/O to sleep across harts without holding a spinlock. Internal callers
  invoke `*_locked` helpers directly to avoid self-deadlock.

- **Allocation & I/O**: `fs_balloc`/`fs_bfree` manage free blocks via block 4;
  `fs_bmap` maps logical file block indices through `direct[]` and `indirect`;
  `fs_read` and `fs_write` handle arbitrary byte offsets and lengths across
  block boundaries; `fs_trunc` frees all data and indirect blocks of an inode.
- **Paths & directories**: `fs_lookup(path)` walks `/` component by component
  (both `"/hello"` and `"hello"` resolve from `/`). `fs_create(path, type)`
  uses `fs_lookup_parent`, allocates a free inode with `fs_ialloc`, initializes
  `.` and `..` for directories, and reuses freed (`inum == 0`) slots in the
  parent directory. `fs_unlink(path)` rejects `/`, `.`, `..`, and non-empty
  directories (`-ERR_NOTEMPTY`), clears the parent directory slot, and frees
  the inode and its blocks with `fs_ifree`.

## Open files (`struct fs_file`)

`fs_files[FS_OPEN_MAX]` (16 slots) wraps on-disk inodes in `struct file` with an
embedded `off` cursor:

- `fs_open(path, flags, &f)` validates `O_RDONLY`, `O_WRONLY`, `O_RDWR`,
  `O_CREAT`, and `O_TRUNC`, creates missing files when `O_CREAT` is set, rejects
  writing or truncating directories (`-ERR_ISDIR`, while allowing `O_RDONLY`
  opens so `ls` can read directory entries), and sets `file.read` / `file.write`
  according to the open mode.
- Once installed in a thread's descriptor table by `open`, `read`, `write`,
  `close`, and `dup` dispatch through `struct file` transparently.
