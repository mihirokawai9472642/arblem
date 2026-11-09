.section .text
.global _start

_start:
    la t0, firmware_trap
    csrw mtvec, t0

    li t0, 0xb1ff
    csrw medeleg, t0
    li t0, 0x222
    csrw mideleg, t0

    li t0, 0x2003ffff
    csrw pmpaddr0, t0
    li t0, -1
    csrw pmpaddr1, t0
    li t0, 0x1f18
    csrw pmpcfg0, t0

    li t0, 2
    csrw mcounteren, t0
    li t0, 1
    slli t0, t0, 63
    csrs menvcfg, t0

    li t0, 0x1800
    csrc mstatus, t0
    li t0, 0x800
    csrs mstatus, t0

    li t0, 0x80200000
    csrw mepc, t0

    mret

.balign 4
firmware_trap:
    wfi
    j firmware_trap
