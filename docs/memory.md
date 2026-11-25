# Memory

Arblem manages memory in two layers. `kernel/page.c` hands out 4 KiB physical
pages, and `kernel/vm.c` maps those pages into Sv39 page tables.

## Physical pages (`kernel/page.c`)

The allocator is a singly linked free list guarded by `page_lock`
(`struct spinlock`). Each free page stores the pointer to the next free page in
its own first 8 bytes, so the list needs no memory of its own. `page_alloc()`
releases `page_lock` before zeroing the popped page so other harts do not spin
during the 4 KiB write loop.

| Function            | Behavior                                                               |
|---------------------|------------------------------------------------------------------------|
| `page_init(start, end)` | Round `start` up to a page boundary and free every whole page below `end` |
| `page_alloc()`      | Pop a page from the list and fill it with zeros. Returns 0 when memory runs out |
| `page_free(p)`      | Push `p` back on the list. Does not check for double frees             |
| `page_free_count()` | Number of pages on the list. The `pages` system call returns this      |

Because `page_alloc` zeroes every page, callers can rely on fresh page tables
being empty and on `.bss` of a new program being zero.

The smoke test uses `page_free_count` to find leaks: the count printed by the
shell's `pages` command must be the same before and after each program runs.

## Sv39 in brief

Sv39 translates 39-bit virtual addresses through three levels of page tables.
Each table is one 4 KiB page that holds 512 entries of 8 bytes.

```
 38        30 29        21 20        12 11           0
+------------+------------+------------+--------------+
|  VPN[2]    |  VPN[1]    |  VPN[0]    |  page offset |
+------------+------------+------------+--------------+
  index into   index into   index into
  level 2      level 1      level 0 (leaf)
```

A page table entry (PTE) holds a physical page number (bits 53:10) and flags:

| Flag | Meaning                                    |
|------|--------------------------------------------|
| `V`  | Entry is valid                             |
| `R`, `W`, `X` | Readable, writable, executable. All three clear means "points to the next level" |
| `U`  | U-mode may access the page                 |
| `A`, `D` | Accessed and dirty                    |

`vm_map` always sets `A` and `D`. Some hardware raises a page fault instead of
setting them itself, and the kernel has no use for them yet.

Arblem only creates 4 KiB leaf pages. It never uses 2 MiB or 1 GiB megapages.

## Kernel mappings (`vm_init`)

`vm_init(const struct fdt_info *info)` builds one kernel page table that
identity-maps (virtual address = physical address) the devices and RAM found in
the device tree:

| Range                                | Permissions | Contents                                         |
|--------------------------------------|-------------|--------------------------------------------------|
| PLIC (from DTB, `0x0c000000` + 6 MiB) | `R W`      | Interrupt controller                             |
| UART (from DTB, `0x10000000` + 4 KiB) | `R W`      | Serial port (rounded outward to 4 KiB page bounds) |
| VirtIO MMIO slots (from DTB, 8 x 4 KiB) | `R W`    | VirtIO MMIO registers                            |
| `__kernel_start` to `__text_end`     | `R X`       | Kernel code                                      |
| `__text_end` to `__rodata_end`       | `R`         | Constants and read-only data                     |
| `__rodata_end` to `ram_end`          | `R W`       | Data, `.bss`, free pages (below/above DTB), and reserved DTB pages |

`vm_map_mmio` rounds MMIO ranges outward to 4 KiB page boundaries (for instance,
the UART's DTB `reg` size is `0x100` bytes). None of these mappings have `U`, so
user programs cannot touch them. The linker script page-aligns `__text_end` and
`__rodata_end` so that the three kernel ranges never share a page, which keeps
the kernel W^X.

`vm_init` finishes by writing `satp` and running `sfence.vma`. The next
instruction fetch is already translated, and works only because the code is
mapped at the same address it was running at.

## User address spaces

`vm_create` makes a new top-level table and copies all 512 entries of the kernel
table into it. The kernel's level 1 and level 0 tables are therefore *shared* by
every address space, and a user address space differs from the kernel one only
in the entries it adds.

All user mappings sit between `0x40000000` and `0x80000000`. That range is
exactly one top-level entry (`VPN[2] = 1`), and the kernel leaves that entry
empty. So:

- Mapping user pages never writes into a shared kernel table.
- `vm_free` can tell user entries from kernel entries by comparing each
  top-level entry with the kernel's. It frees the user's level 1 and level 0
  tables, and every leaf page they point to, because each leaf belongs to that
  one process.

The layout of a user address space:

```
0x00000000 - 0x3fffffff  kernel entry 0: PLIC, UART, and VirtIO MMIO (no U)
0x40000000               .text            R X U
                         .rodata          R U          page aligned
                         .data, .bss      R W U        page aligned
heap_base                heap (sbrk)      R W U        demand paged, up to 0x7ffef000
                         (unmapped)
0x7ffef000               stack guard      unmapped     4 KiB
0x7fff0000               stack (64 KiB)   R W U        demand paged; top 4 KiB mapped at spawn
0x80000000               initial sp top; kernel image and RAM from here (no U)
```

`vm_unmap(root, va)` unmaps a single 4 KiB page, frees its physical page, and
flushes the TLB with `sfence.vma` (used when `sbrk` shrinks the heap).
`vm_is_mapped(root, va)` reports whether a valid leaf mapping exists at `va`.

See [Processes](processes.md) for how the segments and stack are loaded, and
[Traps](traps.md) for demand paging.

## Switching address spaces

`vm_switch(root)` records `root` in `cpu_current()->vm_root` (so each hart tracks
its own active address space), writes `satp`, and flushes the TLB with
`sfence.vma`. Passing 0 selects `vm_kernel_root`, which kernel-only threads (such
as `thread_idle`) and secondary harts entering `kernel_secondary` use.
`thread_sched` calls `vm_switch` on every switch, so each hart's page table
always belongs to the thread running on it.

## Copying to and from user memory

System calls receive user pointers. The kernel must not just dereference them:
the pointer could point at a kernel page, at an unmapped page, or at a page the
program may not write. `vm_copy_from_user` and `vm_copy_to_user` handle this:

1. `vm_check_user` walks `cpu_current()->vm_root` for every page in the range,
   and requires `V`, `U`, and `R` (for reading) or `W` (for writing). When a
   page is not mapped yet, it calls `thread_fault(va)` so an untouched
   demand-paged heap or stack page passed to a system call is mapped on the
   spot. It also rejects ranges that wrap around or leave the Sv39 address
   space.
2. The copy sets `sstatus.SUM` ("permit Supervisor User Memory access") for the
   duration of the copy. Without `SUM`, S-mode faults on any `U` page, which
   protects the kernel from following user pointers by accident.

Both functions return 0 on success and -1 if the check fails. Callers turn -1
into `-ERR_FAULT`. `vm_copy_str_from_user` copies a NUL-terminated string up to
a caller-supplied bound, returning the length, `-ERR_FAULT`, or `-ERR_2BIG`.
