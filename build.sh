#!/bin/sh

set -eu

CFLAGS="--target=riscv64-unknown-elf -mcmodel=medany -ffreestanding -Wall -Wextra"

mkdir -p build

clang $CFLAGS -c src/boot.S -o build/boot.o
clang $CFLAGS -c src/trap.S -o build/trap.o
clang $CFLAGS -c src/kernel.c -o build/kernel.o
ld.lld -T src/linker.ld -o build/kernel.elf build/boot.o build/trap.o build/kernel.o
