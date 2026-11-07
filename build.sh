#!/bin/sh

set -eu

CFLAGS="--target=riscv64-unknown-elf -mcmodel=medany -ffreestanding -Wall -Wextra -Iinc"

rm -rf build
mkdir -p build

for src in src/*.S src/*.c; do
    clang $CFLAGS -c "$src" -o "build/$(basename "$src").o"
done

ld.lld -T src/linker.ld -o build/kernel.elf build/*.o
