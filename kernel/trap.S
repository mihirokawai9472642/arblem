.section .text
.global trap_entry
.global trap_enter_user
.balign 4

trap_entry:
    csrrw sp, sscratch, sp
    bnez sp, 1f
    csrr sp, sscratch

1:
    addi sp, sp, -144

    sd ra, 0(sp)
    sd t0, 8(sp)
    sd t1, 16(sp)
    sd t2, 24(sp)
    sd t3, 32(sp)
    sd t4, 40(sp)
    sd t5, 48(sp)
    sd t6, 56(sp)
    sd a0, 64(sp)
    sd a1, 72(sp)
    sd a2, 80(sp)
    sd a3, 88(sp)
    sd a4, 96(sp)
    sd a5, 104(sp)
    sd a6, 112(sp)
    sd a7, 120(sp)

    csrrw t0, sscratch, zero
    sd t0, 128(sp)

    mv a0, sp
    call trap_handler

    csrr t0, sstatus
    andi t0, t0, 0x100
    bnez t0, 2f

    addi t0, sp, 144
    csrw sscratch, t0

2:
    ld ra, 0(sp)
    ld t0, 8(sp)
    ld t1, 16(sp)
    ld t2, 24(sp)
    ld t3, 32(sp)
    ld t4, 40(sp)
    ld t5, 48(sp)
    ld t6, 56(sp)
    ld a0, 64(sp)
    ld a1, 72(sp)
    ld a2, 80(sp)
    ld a3, 88(sp)
    ld a4, 96(sp)
    ld a5, 104(sp)
    ld a6, 112(sp)
    ld a7, 120(sp)
    ld sp, 128(sp)

    sret

trap_enter_user:
    csrci sstatus, 2

    li t0, 0x100
    csrc sstatus, t0
    csrw sepc, a0

    csrw sscratch, sp
    mv sp, a1

    sret
