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

# riscv-asm-lsp: language server for the assembly sources (kernel/*.S), used by
# Zed (see .zed/settings.json). It is a static musl binary from GitHub Releases,
# pinned by version and SHA-256 so rebuilding the image always gives the same
# server. curl and ca-certificates are only needed for this download.
ARG RISCV_ASM_LSP_VERSION=0.3.0
RUN apt-get update \
    && apt-get install -y --no-install-recommends curl ca-certificates \
    && case "$(dpkg --print-architecture)" in \
        amd64) target=x86_64-unknown-linux-musl \
            sha256=01b70cc21d1269d1c6733d0b41124fcdeb919d088b1e4438ed981274df791f0b ;; \
        arm64) target=aarch64-unknown-linux-musl \
            sha256=fd4314d9d4890d4bb898b371ef4e3f2290cbcb58d7eb40309327746ac19b3115 ;; \
        *) echo "riscv-asm-lsp: unsupported architecture" >&2; exit 1 ;; \
    esac \
    && archive="riscv-asm-lsp-${target}.tar.gz" \
    && curl -fsSLo "/tmp/${archive}" \
        "https://github.com/mihirokawai9472642/riscv-asm-lsp/releases/download/v${RISCV_ASM_LSP_VERSION}/${archive}" \
    && echo "${sha256}  /tmp/${archive}" | sha256sum -c - \
    && tar -xzf "/tmp/${archive}" -C /usr/local/bin riscv-asm-lsp \
    && rm "/tmp/${archive}" \
    && apt-get purge -y --auto-remove curl ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Same UID as the WSL user, so files created in the project keep their owner.
RUN useradd --create-home --uid 1000 --shell /bin/bash dev
USER dev
