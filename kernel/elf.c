#include "elf.h"

int elf_check(const char *image, unsigned long size) {
    const struct elf_header *eh = (const struct elf_header *)image;

    if (size < sizeof(*eh)) {
        return -1;
    }

    if (eh->ident[0] != 0x7f || eh->ident[1] != 'E' || eh->ident[2] != 'L' || eh->ident[3] != 'F') {
        return -1;
    }

    if (eh->ident[4] != ELF_CLASS_64 || eh->ident[5] != ELF_DATA_LSB) {
        return -1;
    }

    if (eh->type != ELF_TYPE_EXEC || eh->machine != ELF_MACHINE_RISCV) {
        return -1;
    }

    if (eh->phentsize != sizeof(struct elf_phdr) || eh->phoff % 8 != 0) {
        return -1;
    }

    if (eh->phoff > size || eh->phnum > (size - eh->phoff) / sizeof(struct elf_phdr)) {
        return -1;
    }

    const struct elf_phdr *ph = (const struct elf_phdr *)(image + eh->phoff);

    for (int i = 0; i < eh->phnum; i++) {
        if (ph[i].type != ELF_PT_LOAD) {
            continue;
        }

        if (ph[i].offset > size || ph[i].filesz > size - ph[i].offset || ph[i].filesz > ph[i].memsz) {
            return -1;
        }
    }

    return 0;
}
