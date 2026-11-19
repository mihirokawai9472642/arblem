# Development container for Arblem (RISC-V 64 OS), built with the LLVM
# toolchain and run on QEMU.
FROM debian:trixie

# clang/lld/llvm: cross compiler, linker and binary tools (llvm-objdump, ...)
# lldb:           debugger, attached to QEMU's gdbstub (make debug/lldb)
# clangd:         language server used by Zed (see .zed/settings.json)
# clang-tidy/clang-format: static analysis and formatting (make tidy/format)
# qemu-system-riscv: QEMU with the OpenSBI firmware for the virt machine
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        clang lld llvm lldb clang-tidy clang-format clangd make \
        qemu-system-riscv \
    && rm -rf /var/lib/apt/lists/*

# Same UID as the WSL user, so files created in the project keep their owner.
RUN useradd --create-home --uid 1000 --shell /bin/bash dev
USER dev
