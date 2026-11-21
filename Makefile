# Build the kernel and user programs into build/, mirroring the source tree
# (kernel/trap.S -> build/kernel/trap.S.o). See docs/build-and-test.md.

# Target: 64-bit RISC-V, bare metal. medany lets code and data be linked at
# any address (0x80200000 for the kernel, 0x40000000 for user programs).
TARGET  := --target=riscv64-unknown-elf -march=rv64gc -mabi=lp64d -mcmodel=medany

BUILD   := build
KERNEL  := $(BUILD)/kernel.elf
DISK    := $(BUILD)/disk.img

CC      := clang
LD      := ld.lld
QEMU    := qemu-system-riscv64

# -ffreestanding: no hosted C library; only the compiler's own headers are
# available. -g emits debug info for LLDB.
CFLAGS    := $(TARGET) -std=c17 -Wall -Wextra -g -ffreestanding -Iinc
ASFLAGS   := $(TARGET) -g -Iinc

# One hart on the virt machine, OpenSBI as firmware, console on stdio.
# Force modern VirtIO 1.0+ (MMIO v2) instead of legacy v1.
QEMUFLAGS := -machine virt -nographic -bios default \
             -global virtio-mmio.force-legacy=false
BLKFLAGS  := -drive file=$(DISK),if=none,format=raw,id=hd0 \
             -device virtio-blk-device,drive=hd0,bus=virtio-mmio-bus.0

# Programs embedded in the kernel. Each also needs an entry in kernel/user.S
# and kernel/proc.c; see docs/user.md.
USER_PROGS := hello count fault orphan regcheck regclobber pipecheck upper echo memcheck shell

KERNEL_OBJS := $(patsubst %,$(BUILD)/%.o,$(wildcard kernel/*.S kernel/*.c))
LIB_OBJS    := $(patsubst %,$(BUILD)/%.o,$(wildcard lib/*.c))
USER_OBJS   := $(patsubst %,$(BUILD)/%.o,$(wildcard user/*.c))
ALL_OBJS    := $(KERNEL_OBJS) $(LIB_OBJS) $(USER_OBJS)
USER_ELFS   := $(patsubst %,$(BUILD)/user/%.elf,$(USER_PROGS))

C_SOURCES := $(wildcard kernel/*.c lib/*.c user/*.c)
C_HEADERS := $(wildcard kernel/*.h inc/*.h user/*.h)

# Only the rules below apply; make's implicit rules would get in the way.
MAKEFLAGS += --no-builtin-rules

.PHONY: all run test debug lldb clean format tidy

all: $(KERNEL) $(BUILD)/compile_commands.json

run: $(KERNEL) $(DISK)
	$(QEMU) $(QEMUFLAGS) $(BLKFLAGS) -kernel $(KERNEL)

test: $(KERNEL) $(DISK)
	sh test/smoke.sh $(QEMU) $(QEMUFLAGS) $(BLKFLAGS) -kernel $(KERNEL)

# Boot halted at the first instruction, waiting for a debugger on port 1234.
# Attach with "make lldb" from another terminal.
debug: $(KERNEL) $(DISK)
	$(QEMU) $(QEMUFLAGS) $(BLKFLAGS) -kernel $(KERNEL) -s -S

$(DISK):
	@mkdir -p $(dir $@)
	truncate -s 1M $@

lldb: $(KERNEL)
	lldb $(KERNEL) -o 'gdb-remote 1234'

clean:
	rm -rf $(BUILD)

# Formatting and static analysis follow .clang-format and .clang-tidy.
# Assembly files are not covered.
format:
	clang-format -i $(C_SOURCES) $(C_HEADERS)

tidy: $(BUILD)/compile_commands.json
	clang-tidy -p $(BUILD) $(C_SOURCES)

# -MMD -MP: record header dependencies, so editing a header rebuilds its users.
# -MJ: write a compilation database entry for clangd next to each object.
$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MJ $@.json -c $< -o $@

$(BUILD)/%.S.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -MMD -MP -MJ $@.json -c $< -o $@

# Join the entries into build/compile_commands.json, which clangd finds on its
# own. The compiler flags are written only once, in this Makefile.
$(BUILD)/compile_commands.json: $(ALL_OBJS)
	sed -e '1s/^/[\n/' -e '$$s/,$$/\n]/' $(ALL_OBJS:=.json) > $@

$(KERNEL): kernel/linker.ld $(KERNEL_OBJS) $(LIB_OBJS)
	$(LD) -T kernel/linker.ld -o $@ $(KERNEL_OBJS) $(LIB_OBJS)

# kernel/user.S pulls the user ELF files in with .incbin.
$(BUILD)/kernel/user.S.o: $(USER_ELFS)

# Each program: the shared entry point, its own object, and lib/.
$(USER_ELFS): $(BUILD)/user/%.elf: user/linker.ld $(BUILD)/user/start.c.o $(BUILD)/user/%.c.o $(LIB_OBJS)
	$(LD) -T user/linker.ld -o $@ $(filter %.o,$^)

-include $(ALL_OBJS:.o=.d)
