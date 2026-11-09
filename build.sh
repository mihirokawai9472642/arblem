#!/bin/sh

set -eu

CFLAGS="--target=riscv64-unknown-elf -mcmodel=medany -ffreestanding -Wall -Wextra -Iinc"

rm -rf build

for src in firmware/*.S src/*.S src/*.c lib/*.c user/*.c; do
    mkdir -p "build/$(dirname "$src")"
    clang $CFLAGS -c "$src" -o "build/$src.o"
done

ld.lld -T firmware/linker.ld -o build/firmware.elf build/firmware/*.o
ld.lld -T src/linker.ld -o build/kernel.elf build/src/*.o build/lib/*.o build/user/*.o
