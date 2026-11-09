#!/bin/sh

exec qemu-system-riscv64 -machine virt -bios build/firmware.elf -nographic -kernel build/kernel.elf
