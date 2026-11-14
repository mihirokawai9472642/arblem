# Arblem

Just a hobby operating system :)

## Environment
- Clang 19
- LLD 19
- LLVM 19
- LLDB 19
- QEMU 10 (with OpenSBI 1.6)
- GNU Make 4.4

Install on Debian 13 Trixie:

```sh
sudo apt install clang lld llvm lldb make qemu-system-riscv
```

## Usage

| Command      | Description                             |
|--------------|-----------------------------------------|
| `make`       | Build the kernel and user programs      |
| `make run`   | Boot the kernel on OpenSBI              |
| `make test`  | Run the smoke test                      |
| `make clean` | Remove `build/`                         |

To quit QEMU, type `poweroff` in the shell, or press `Ctrl-A` and then `X`.

The shell accepts `help`, `pages`, `ticks` and `poweroff`, and runs the user programs `hello`, `count`, and `fault` by name.

## Structure

| Directory   | Contents                                                  |
|-------------|-----------------------------------------------------------|
| `kernel/`   | S-mode kernel and its private headers                     |
| `user/`     | U-mode programs and their system call wrappers            |
| `lib/`      | Code linked into both the kernel and user programs        |
| `inc/`      | Headers shared by more than one program                   |
| `test/`     | Smoke test run by `make test`                             |
