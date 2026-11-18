#pragma once

// The parts of the 64-bit ELF format that the program loader uses.

// Values of elf_header fields that elf_check accepts.
#define ELF_CLASS_64      2    // ident[4]: 64-bit objects
#define ELF_DATA_LSB      1    // ident[5]: little-endian
#define ELF_TYPE_EXEC     2    // type: executable at a fixed address
#define ELF_MACHINE_RISCV 243  // machine: RISC-V

// elf_phdr.type of a segment to load into memory.
#define ELF_PT_LOAD       1

// elf_phdr.flags: segment permissions.
#define ELF_PF_X 1
#define ELF_PF_W 2
#define ELF_PF_R 4

// File header, at offset 0 of the file.
struct elf_header {
    unsigned char ident[16];  // Magic "\x7fELF", class, data encoding, ...
    unsigned short type;
    unsigned short machine;
    unsigned int version;
    unsigned long entry;      // Virtual address of the first instruction
    unsigned long phoff;      // File offset of the program header table
    unsigned long shoff;
    unsigned int flags;
    unsigned short ehsize;
    unsigned short phentsize; // Size of one program header
    unsigned short phnum;     // Number of program headers
    unsigned short shentsize;
    unsigned short shnum;
    unsigned short shstrndx;
};

// Program header: describes one segment.
struct elf_phdr {
    unsigned int type;
    unsigned int flags;
    unsigned long offset;     // File offset of the segment's data
    unsigned long vaddr;      // Virtual address to load it at
    unsigned long paddr;
    unsigned long filesz;     // Bytes of data in the file
    unsigned long memsz;      // Bytes in memory; the rest past filesz is zero
    unsigned long align;
};

// Check that image (size bytes) is an ELF executable for this machine and that
// every header and PT_LOAD segment lies inside the image. Returns 0 if so,
// -1 otherwise.
int elf_check(const char *image, unsigned long size);
