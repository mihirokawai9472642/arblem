// ELF validation. The loader in proc.c reads headers from the filesystem and
// bounds-checks every offset and size here before trusting them.

#include "elf.h"

int elf_check_header(const struct elf_header *eh, unsigned long size) {
    if (size < sizeof(*eh)) {
        return -1;
    }

    if (eh->ident[0] != 0x7f || eh->ident[1] != 'E' || eh->ident[2] != 'L' ||
        eh->ident[3] != 'F') {
        return -1;
    }

    if (eh->ident[4] != ELF_CLASS_64 || eh->ident[5] != ELF_DATA_LSB) {
        return -1;
    }

    if (eh->type != ELF_TYPE_EXEC || eh->machine != ELF_MACHINE_RISCV) {
        return -1;
    }

    if (eh->phentsize != sizeof(struct elf_phdr)) {
        return -1;
    }

    // Written so that no addition can overflow.
    if (eh->phoff > size ||
        eh->phnum > (size - eh->phoff) / sizeof(struct elf_phdr)) {
        return -1;
    }

    return 0;
}

int elf_check_phdr(const struct elf_phdr *ph, unsigned long size) {
    if (ph->type != ELF_PT_LOAD) {
        return 0;
    }

    if (ph->offset > size || ph->filesz > size - ph->offset ||
        ph->filesz > ph->memsz) {
        return -1;
    }

    return 0;
}
