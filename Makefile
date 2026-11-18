# Build the kernel and user programs into build/, mirroring the source tree
# (kernel/trap.S -> build/kernel/trap.S.o). See docs/build-and-test.md.

CC      := clang
LD      := ld.lld
QEMU    := qemu-system-riscv64

# Bare-metal RV64 code that can be linked at any address (medany), without a
# hosted C library (freestanding). -MMD -MP write .d files for header changes.
CFLAGS    := --target=riscv64-unknown-elf -mcmodel=medany -ffreestanding -Wall -Wextra -Iinc -MMD -MP

# One hart on the virt machine, OpenSBI as firmware, console on stdio.
QEMUFLAGS := -machine virt -nographic -bios default -kernel build/kernel.elf

# Programs embedded in the kernel. Each also needs an entry in kernel/user.S
# and kernel/proc.c; see docs/user.md.
USER_PROGS := hello count fault orphan shell

KERNEL_OBJS := $(patsubst %,build/%.o,$(wildcard kernel/*.S kernel/*.c))
LIB_OBJS    := $(patsubst %,build/%.o,$(wildcard lib/*.c))
USER_OBJS   := $(patsubst %,build/%.o,$(wildcard user/*.c))
USER_ELFS   := $(patsubst %,build/user/%.elf,$(USER_PROGS))

# Only the rules below apply; make's implicit rules would get in the way.
MAKEFLAGS += --no-builtin-rules

.PHONY: all run test clean

all: build/kernel.elf

run: all
	$(QEMU) $(QEMUFLAGS)

test: all
	sh test/smoke.sh $(QEMU) $(QEMUFLAGS)

clean:
	rm -rf build

build/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.S.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/kernel.elf: kernel/linker.ld $(KERNEL_OBJS) $(LIB_OBJS)
	$(LD) -T kernel/linker.ld -o $@ $(KERNEL_OBJS) $(LIB_OBJS)

# kernel/user.S pulls the user ELF files in with .incbin.
build/kernel/user.S.o: $(USER_ELFS)

# Each program: the shared entry point, its own object, and lib/.
$(USER_ELFS): build/user/%.elf: user/linker.ld build/user/start.c.o build/user/%.c.o $(LIB_OBJS)
	$(LD) -T user/linker.ld -o $@ $(filter %.o,$^)

-include $(KERNEL_OBJS:.o=.d) $(LIB_OBJS:.o=.d) $(USER_OBJS:.o=.d)
