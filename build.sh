#!/bin/sh

set -eu

CFLAGS="--target=riscv64-unknown-elf -mcmodel=medany -ffreestanding -Wall -Wextra -Iinc"
USER_PROGS="hello count"

rm -rf build

compile() {
    mkdir -p "build/$(dirname "$1")"
    clang $CFLAGS -c "$1" -o "build/$1.o"
}

for src in user/*.c; do
    compile "$src"
done

for prog in $USER_PROGS; do
    ld.lld -T user/linker.ld -o "build/user/$prog.elf" build/user/start.c.o "build/user/$prog.c.o"
    llvm-objcopy --set-section-flags .bss=alloc,contents -O binary "build/user/$prog.elf" "build/user/$prog.bin"
done

for src in firmware/*.S src/*.S src/*.c lib/*.c; do
    compile "$src"
done

ld.lld -T firmware/linker.ld -o build/firmware.elf build/firmware/*.o
ld.lld -T src/linker.ld -o build/kernel.elf build/src/*.o build/lib/*.o
