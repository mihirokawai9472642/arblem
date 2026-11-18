# Processes

A process in Arblem is a thread that runs a user program in its own address
space. `kernel/proc.c` creates processes, and `kernel/thread.c` schedules,
ends and reclaims them (see [Threads](threads.md)).

## Where programs come from

There is no file system yet. Every user program is linked into the kernel image:

1. `make` builds each program in `USER_PROGS` as its own ELF file,
   `build/user/<name>.elf`.
2. `kernel/user.S` includes each ELF file byte for byte with `.incbin`, in
   `.rodata`, between the symbols `user_<name>` and `user_<name>_end`.
3. `proc_images[]` in `kernel/proc.c` maps each program name to its two
   symbols.

`proc_spawn(name)` looks the name up in that table. An unknown name gives
`-ERR_NOENT`.

## Checking the ELF file (`kernel/elf.c`)

`elf_check(image, size)` accepts only what the loader can handle:

- ELF magic, 64-bit class, little-endian.
- Type `ET_EXEC` (a static executable at a fixed address), machine RISC-V.
- Program headers of the expected size, 8-byte aligned, entirely inside the
  image.
- For each `PT_LOAD` segment: the file data lies inside the image, and
  `filesz <= memsz`.

The loader trusts these fields after the check, so every bound it relies on is
checked here first. The embedded programs are built by the same Makefile, but
the checks keep a broken build from corrupting kernel memory.

## Loading (`proc_create`)

1. `vm_create()` makes a new page table that shares the kernel mappings.
2. For each `PT_LOAD` segment, `proc_load`:
   - Rejects the segment unless its `vaddr` is page aligned, does not overlap
     the previous segment, and ends below the stack page.
   - Rejects segments that are both writable and executable (W^X).
   - Allocates and maps one zeroed page at a time, then copies `filesz` bytes
     from the image. The rest of the segment (`.bss`) stays zero.
3. Maps one stack page just below `0x80000000`.
4. Creates a thread with `proc_entry` as its entry function and the ELF entry
   point as its argument.

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
calls `trap_enter_user(entry, 0x80000000)`. The thread is in U-mode from then
on, and only comes back into the kernel through traps. The kernel stack stays
with the thread, and serves every trap it takes.

## Lifetime from user space

| Step | System call            | Kernel side                                 |
|------|------------------------|---------------------------------------------|
| 1    | `spawn(name, len)`     | `proc_spawn` returns the new thread ID (PID) |
| 2    | (program runs)         | Scheduled with every other thread           |
| 3    | `exit(code)` or a fault | `thread_exit(code)`; a fault exits with -1 |
| 4    | `wait(pid, &status)`   | `thread_wait` frees the child, returns the code |

`user/start.c` calls `exit` with `main`'s return value, so a program that
returns from `main` exits normally.

Only the parent can `wait` for a child. PIDs are slot numbers and are reused as
soon as `wait` frees a slot.

## Adding a program

See [User programs](user.md#adding-a-program).
