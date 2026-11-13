#pragma once

#define ELF_CLASS_64      2
#define ELF_DATA_LSB      1
#define ELF_TYPE_EXEC     2
#define ELF_MACHINE_RISCV 243
#define ELF_PT_LOAD       1

#define ELF_PF_X 1
#define ELF_PF_W 2
#define ELF_PF_R 4

struct elf_header {
    unsigned char ident[16];
    unsigned short type;
    unsigned short machine;
    unsigned int version;
    unsigned long entry;
    unsigned long phoff;
    unsigned long shoff;
    unsigned int flags;
    unsigned short ehsize;
    unsigned short phentsize;
    unsigned short phnum;
    unsigned short shentsize;
    unsigned short shnum;
    unsigned short shstrndx;
};

struct elf_phdr {
    unsigned int type;
    unsigned int flags;
    unsigned long offset;
    unsigned long vaddr;
    unsigned long paddr;
    unsigned long filesz;
    unsigned long memsz;
    unsigned long align;
};

int elf_check(const char *image, unsigned long size);
