# Arblem

Just a hobby operating system :)

## Environment
- Clang 19
- LLD 19
- LLVM 19
- LLDB 19
- QEMU 10
- GNU Make 4.4

Install on Debian 13 Trixie:

```sh
sudo apt install clang lld llvm lldb make qemu-system-riscv
```

## Usage

| Command            | Description                                       |
|--------------------|---------------------------------------------------|
| `make`             | Build the firmware, the kernel and user programs  |
| `make run`         | Boot with the Arblem firmware                     |
| `make run-opensbi` | Boot with OpenSBI instead of the Arblem firmware  |
| `make test`        | Run the smoke test with both firmwares            |
| `make clean`       | Remove `build/`                                   |

To quit QEMU, press `Ctrl-A` and then `X`.

The shell accepts `help`, `pages` and `ticks`, and runs the user programs `hello` and `count` by name.

## Structure

| Directory   | Contents                                                  |
|-------------|-----------------------------------------------------------|
| `firmware/` | M-mode firmware that starts the kernel                    |
| `kernel/`   | S-mode kernel and its private headers                     |
| `user/`     | U-mode programs and their system call wrappers            |
| `lib/`      | Code linked into both the kernel and user programs        |
| `inc/`      | Headers shared by more than one program                   |
| `test/`     | Smoke test run by `make test`                             |
