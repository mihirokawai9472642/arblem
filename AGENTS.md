# AGENTS.md

Instructions for coding agents working in this repository. `README.md` is the
documentation for people, and `docs/` explains how the kernel works.

## Project overview

Arblem is a small freestanding operating system for 64-bit RISC-V (`rv64gc`,
`lp64d`), written in C17 and assembly and built with the LLVM toolchain
(`clang`, `ld.lld`). It boots on the QEMU `virt` machine through OpenSBI (which
jumps to the kernel at `0x80200000` in S-mode), runs user programs in U-mode in
separate Sv39 address spaces, and provides a tiny shell. There is no hosted C
library; only the compiler's own headers (`stdint.h`, `stddef.h`, ...) and the
project's shared headers in `inc/` are available.

- `kernel/`: S-mode kernel, assembly entry/trap/context-switch code,
  `kernel/linker.ld`, and private headers.
- `user/`: U-mode programs, `_start` runtime (`user/start.c`), system call
  wrappers (`user/sys.h`), and `user/linker.ld`.
- `lib/`: shared helpers linked into both the kernel and user programs.
- `inc/`: headers shared across `kernel/`, `user/`, and `lib/`.
- `tools/`: host build utilities (`tools/mkfs.c` builds filesystem images).
- `test/`: smoke test (`test/smoke.sh`) run by `make test`.
- `docs/`: design documentation; start from `docs/README.md` and
  `docs/architecture.md` (which states the kernel-wide design rules: multi-hart
  SMP with spinlocks and sleeplocks, interrupts off in the kernel,
  identity-mapped kernel memory, user memory in one 1 GiB slot, W^X, and
  negative `-ERR_*` error codes).
- `build/`: everything generated (`kernel.elf`, user ELFs, `mkfs`, `disk.img`,
  `compile_commands.json`, test logs); not in git.

## Commands

The cross compiler, QEMU, and the other tools run in a container, not in WSL.
Run every command from the project root; `compose.yaml` mounts the project at
the same path inside the container, so paths in compiler output are valid on
both sides.

- `docker compose up -d --build`: builds and starts the container. The other
  commands need it running.
- `docker compose exec dev make`: builds `build/kernel.elf`, the user ELFs
  (`build/user/*.elf`), `build/mkfs`, `build/disk.img`, and
  `build/compile_commands.json`.
- `docker compose exec dev make test`: builds if needed and runs the smoke test
  (`test/smoke.sh`) in QEMU, ending with `poweroff`.
- `docker compose exec dev make run`: boots the kernel in QEMU with the serial
  console on standard output. QEMU stays in the shell until `poweroff` or
  Ctrl-A, then X, so when running it without a person, put a time limit on it:
  `docker compose exec dev timeout 5 make run`. The output up to the limit is
  kept; exit status 124 only means the limit was reached. Prefer `make test` for
  automated checks.
- `docker compose exec dev make format`: formats all C sources and headers
  (`kernel/`, `lib/`, `inc/`, `user/`, `tools/`) with `clang-format`. Assembly
  is not covered.
- `docker compose exec dev make tidy`: checks all C sources (`kernel/`, `lib/`,
  `user/`) with `clang-tidy`.
- `docker compose exec dev make clean`: deletes `build/`.
- `make debug` and `make lldb` are for a person debugging interactively in two
  terminals; do not start them unless asked.

Do not install compilers, QEMU, or other project tools in WSL. Add them to the
`Dockerfile` and rebuild the container with `docker compose up -d --build`.

Run `git` in WSL, not in the container: commit signing and SSH are not
available there.

## Code style

- C17, freestanding. Formatting follows `.clang-format`; run `make format`
  after editing C sources or headers.
- Build flags are written only in the `Makefile`; `clangd` and `clang-tidy`
  read them from the generated `build/compile_commands.json`.
- Files, comments, and commit messages are in English.
- Comments explain why, in plain sentences.
- Keep `docs/` and `README.md` current: when a change makes a page wrong,
  update that page in the same change.

## Testing

- `make` builds with no warnings.
- `make tidy` reports no warnings in the project's own files (warnings it
  suppresses in system headers, or explicitly justified `NOLINT` annotations
  such as the intentional page fault in `user/fault.c`, do not count).
- `make test` passes every check (`boot`, `smp_boot`, `hello`, `count`, `fault`,
  `unknown`, `regs`, `pipe`, `pipeline`, `args`, `mem`, `blk`, `fs`, `cat`,
  `ls`, `smp`, `blk_persist`, `fs_persist`, `orphan`, `pages`, `poweroff`).

## Git workflow

The project follows GitHub Flow: `main` always builds and passes `make test`,
and every change reaches it through a pull request.

- Make each change on its own branch created from an up-to-date `main`, named
  after the change with the commit type as a prefix (for example
  `feat/uart-input`). Do not commit to `main` directly.
- Push the branch and open a pull request only when the user asks. The pull
  request says what changed and why, and how it was tested; when a coding agent
  contributed, it says so there as well.
- Merging happens through the pull request after review; delete the branch
  afterwards.

## Commits

- Message format: `type(scope): summary` following Conventional Commits
  (`feat`, `fix`, `refactor`, `docs`, `test`, `build`, `style`, `chore`, ...),
  then a short body when needed.
- When a coding agent contributed to a commit, add a trailer naming it, with
  the model when known:
  - `Assisted-By: <tool> [model]` when a person materially revised the work.
  - `Generated-By: <tool> [model]` when it was mostly written by the agent.

  Do not use `Co-Authored-By` for AI tools; it is meant for human co-authors.
