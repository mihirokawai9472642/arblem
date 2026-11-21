// Minimal Flattened Device Tree (FDT) parser. Reads memory range, timer
// frequency, PLIC, UART, and VirtIO MMIO slots from the DTB passed by OpenSBI.

#include "fdt.h"
#include "str.h"

#define FDT_MAGIC      0xd00dfeedU
#define FDT_HEADER_LEN 40U

#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE   2U
#define FDT_PROP       3U
#define FDT_NOP        4U
#define FDT_END        9U

// Read big-endian 32-bit and 64-bit integers from unaligned byte pointers.
static unsigned int be32(const unsigned char *p) {
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

static unsigned long be64(const unsigned char *p) {
    return ((unsigned long)be32(p) << 32) | (unsigned long)be32(p + 4);
}

// Round n up to a 4-byte FDT token boundary.
static unsigned int fdt_align4(unsigned int n) {
    return (n + 3U) & ~3U;
}

// Check whether the NUL-separated string list prop[0..len) contains target.
static int fdt_compat_has(const char *prop, unsigned int len,
                          const char *target) {
    unsigned int off = 0;

    while (off < len) {
        unsigned int n = 0;

        while (off + n < len && prop[off + n] != '\0') {
            n++;
        }

        if (off + n < len && str_equal(prop + off, target)) {
            return 1;
        }

        off += n + 1;
    }

    return 0;
}

// Properties collected for the current leaf node before FDT_END_NODE commits
// them (in QEMU's DTB, reg and interrupts appear before compatible).
struct fdt_node {
    unsigned long reg_base;
    unsigned long reg_size;
    unsigned int irq;
    int has_reg;
    int is_memory;
    int is_plic;
    int is_uart;
    int is_virtio;
};

static void fdt_node_reset(struct fdt_node *n) {
    n->reg_base = 0;
    n->reg_size = 0;
    n->irq = 0;
    n->has_reg = 0;
    n->is_memory = 0;
    n->is_plic = 0;
    n->is_uart = 0;
    n->is_virtio = 0;
}

static void fdt_node_commit(const struct fdt_node *n, struct fdt_info *out) {
    if (!n->has_reg) {
        return;
    }

    if (n->is_memory && out->ram_size == 0) {
        out->ram_base = n->reg_base;
        out->ram_size = n->reg_size;
    }

    if (n->is_plic && out->plic.size == 0) {
        out->plic.base = n->reg_base;
        out->plic.size = n->reg_size;
    }

    if (n->is_uart && out->uart.size == 0) {
        out->uart.base = n->reg_base;
        out->uart.size = n->reg_size;
        out->uart.irq = n->irq;
    }

    if (n->is_virtio && out->virtio_count < FDT_VIRTIO_MAX) {
        struct fdt_mmio *slot = &out->virtio[out->virtio_count++];

        slot->base = n->reg_base;
        slot->size = n->reg_size;
        slot->irq = n->irq;
    }
}

int fdt_parse(unsigned long dtb, struct fdt_info *out) {
    const unsigned char *base = (const unsigned char *)dtb;

    if (be32(base + 0) != FDT_MAGIC) {
        return -1;
    }

    unsigned int totalsize = be32(base + 4);
    unsigned int off_struct = be32(base + 8);
    unsigned int off_strings = be32(base + 12);
    unsigned int size_strings = be32(base + 32);
    unsigned int size_struct = be32(base + 36);

    if (totalsize < FDT_HEADER_LEN || off_struct > totalsize ||
        size_struct > totalsize - off_struct || off_strings > totalsize ||
        size_strings > totalsize - off_strings) {
        return -1;
    }

    // Zero *out byte by byte to avoid freestanding memset calls.
    unsigned char *dst = (unsigned char *)out;

    for (unsigned long i = 0; i < sizeof(*out); i++) {
        dst[i] = 0;
    }

    out->dtb_size = totalsize;

    const unsigned char *sb = base + off_struct;
    const char *strings = (const char *)(base + off_strings);
    unsigned int p = 0;
    struct fdt_node cur;

    fdt_node_reset(&cur);

    while (p + 4 <= size_struct) {
        unsigned int token = be32(sb + p);

        p += 4;

        if (token == FDT_BEGIN_NODE) {
            // Skip NUL-terminated node name and align to 4 bytes.
            unsigned int n = 0;

            while (p + n < size_struct && sb[p + n] != '\0') {
                n++;
            }

            if (p + n >= size_struct) {
                return -1;
            }

            p += fdt_align4(n + 1);
            fdt_node_reset(&cur);
            continue;
        }

        if (token == FDT_END_NODE) {
            fdt_node_commit(&cur, out);
            fdt_node_reset(&cur);
            continue;
        }

        if (token == FDT_PROP) {
            if (p + 8 > size_struct) {
                return -1;
            }

            unsigned int plen = be32(sb + p);
            unsigned int nameoff = be32(sb + p + 4);

            p += 8;

            if (plen > size_struct - p) {
                return -1;
            }

            unsigned int padded = fdt_align4(plen);

            if (padded > size_struct - p || nameoff >= size_strings) {
                return -1;
            }

            const unsigned char *val = sb + p;
            const char *pname = strings + nameoff;

            p += padded;

            if (str_equal(pname, "reg") && plen >= 16) {
                cur.reg_base = be64(val);
                cur.reg_size = be64(val + 8);
                cur.has_reg = 1;
            } else if (str_equal(pname, "interrupts") && plen >= 4) {
                cur.irq = be32(val);
            } else if (str_equal(pname, "device_type") &&
                       fdt_compat_has((const char *)val, plen, "memory")) {
                cur.is_memory = 1;
            } else if (str_equal(pname, "compatible")) {
                if (fdt_compat_has((const char *)val, plen, "riscv,plic0") ||
                    fdt_compat_has((const char *)val, plen,
                                   "sifive,plic-1.0.0")) {
                    cur.is_plic = 1;
                } else if (fdt_compat_has((const char *)val, plen,
                                          "ns16550a")) {
                    cur.is_uart = 1;
                } else if (fdt_compat_has((const char *)val, plen,
                                          "virtio,mmio")) {
                    cur.is_virtio = 1;
                }
            } else if (str_equal(pname, "timebase-frequency")) {
                if (plen == 4) {
                    out->timebase_freq = be32(val);
                } else if (plen == 8) {
                    out->timebase_freq = be64(val);
                }
            }

            continue;
        }

        if (token == FDT_NOP) {
            continue;
        }

        if (token == FDT_END) {
            return 0;
        }

        return -1;
    }

    return -1;
}
