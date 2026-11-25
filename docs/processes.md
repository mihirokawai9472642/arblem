# Processes

A process in Arblem is a thread that runs a user program in its own address
space. `kernel/proc.c` creates processes, and `kernel/thread.c` schedules,
ends and reclaims them (see [Threads](threads.md)).

## Where programs come from

Every user program is stored as an ELF file on the VirtIO-blk filesystem:

1. `make` builds each program in `USER_PROGS` as `build/user/<name>.elf`.
2. `build/mkfs` packs all user ELFs into the root directory (`/`) of the disk
   image (`build/disk.img`, or `build/test.img` for `make test`) under `<name>`.
3. `proc_spawn(name, argc, argv)` resolves `name` with `fs_lookup(name)` (an
   unknown name gives `-ERR_NOENT`) and checks `fs_stat` to confirm it is a
   regular file (`FS_TYPE_FILE`; directories return `-ERR_NOEXEC`).

## Checking the ELF file (`kernel/elf.c`)

`kernel/proc.c` streams headers directly from the filesystem rather than loading
the whole ELF file into a kernel buffer. `elf_check_header(eh, size)` and
`elf_check_phdr(ph, size)` accept only what the loader can handle:

- ELF magic, 64-bit class, little-endian.
- Type `ET_EXEC` (a static executable at a fixed address), machine RISC-V.
- Program headers of the expected size (`sizeof(struct elf_phdr)`), with the
  program header table lying inside the `size` bytes reported by `fs_stat`.
- For each `PT_LOAD` segment: the file data (`[offset, offset + filesz)`) lies
  inside `size`, and `filesz <= memsz`.

The loader trusts these fields after the checks, so every bound it relies on is
verified before mapping pages.

## Loading (`proc_create`)

1. Reads the ELF header from offset 0 of `inum` with `fs_read`, validates it
   with `elf_check_header`, and creates a new page table sharing kernel mappings
   with `vm_create()`.
2. Reads each program header from `inum` with `fs_read`, validates it with
   `elf_check_phdr`, and for each `PT_LOAD` segment calls `proc_load`:
   - Rejects the segment unless its `vaddr` is page aligned, does not overlap
     the previous segment, and ends at or below `PROC_HEAP_LIMIT` (`0x7ffef000`,
     leaving room for a 64 KiB stack and its 4 KiB guard page).
   - Rejects segments that are both writable and executable (W^X).
   - Allocates and maps one zeroed page at a time, then reads up to `PAGE_SIZE`
     bytes of `filesz` directly from `inum` into that page via `fs_read`.
     Bytes past `filesz` (`.bss`) stay zero, and non-loadable sections such as
     debug info are never read.
   - Advances `next` (the page-aligned address right after the segments), which
     becomes the process's `heap_base`.
3. Maps the top stack page (`0x7ffff000`-`0x80000000`) and calls
   `proc_build_stack` to copy the `argv` strings and NULL-terminated pointer
   array there, 16-byte aligned for the `lp64d` ABI. When `argc == 0` or
   `argv == 0`, `proc_spawn` passes `{name}` so `argv[0]` is always set.
4. Creates a thread with `proc_entry` as its entry function and the ELF entry
   point as its argument, stores the initial `sp`, `argc`, and `argv` with
   `thread_set_start` and the initial heap break with `thread_set_heap`, and
   finally marks it runnable with `thread_ready(id)` so another hart cannot
   pick it before setup finishes.

If any step fails, `vm_free` releases the page table and every page mapped so
far, and the error is returned.

Segment permissions come from the ELF flags:

| ELF flags | PTE flags   | Section  |
|-----------|-------------|----------|
| `R X`     | `R X U`     | `.text`  |
| `R`       | `R U`       | `.rodata` |
| `R W`     | `R W U`     | `.data`, `.bss` |

RISC-V has no write-only pages, so a writable segment is always made readable
as well.

`user/linker.ld` places `.text` at `0x40000000` and page-aligns `.rodata` and
`.data`, so each segment starts on its own page. See
[Memory](memory.md#user-address-spaces) for the full layout.

## Starting

The new thread begins in `thread_entry`, which calls `proc_entry(entry)`, which
reads the saved `sp`, `argc`, and `argv` via `thread_get_start` and calls
`trap_enter_user(entry, sp, argc, argv)`. The thread is in U-mode from then on,
and only comes back into the kernel through traps (system calls, interrupts, or
demand page faults). The kernel stack stays with the thread, and serves every
trap it takes.

## Lifetime from user space

| Step | System call              | Kernel side                                 |
|------|--------------------------|---------------------------------------------|
| 1    | `spawn(name, len, argv)` | `proc_spawn` returns the new thread ID (PID) |
| 2    | (program runs)           | Scheduled with every other thread           |
| 3    | `exit(code)` or a fault  | `thread_exit(code)`; a fault exits with -1 |
| 4    | `wait(pid, &status)`     | `thread_wait` frees the child, returns the code |

`user/start.c` calls `exit` with `main`'s return value, so a program that
returns from `main` exits normally.

Only the parent can `wait` for a child. PIDs are slot numbers and are reused as
soon as `wait` frees a slot.

## Adding a program

See [User programs](user.md#adding-a-program).
