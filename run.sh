#!/bin/sh

exec qemu-system-riscv64 -machine virt -bios none -nographic -kernel build/kernel.elf
