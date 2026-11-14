CC      := clang
LD      := ld.lld
QEMU    := qemu-system-riscv64

CFLAGS    := --target=riscv64-unknown-elf -mcmodel=medany -ffreestanding -Wall -Wextra -Iinc -MMD -MP
QEMUFLAGS := -machine virt -nographic -bios default -kernel build/kernel.elf

USER_PROGS := hello count fault shell

KERNEL_OBJS := $(patsubst %,build/%.o,$(wildcard kernel/*.S kernel/*.c))
LIB_OBJS    := $(patsubst %,build/%.o,$(wildcard lib/*.c))
USER_OBJS   := $(patsubst %,build/%.o,$(wildcard user/*.c))
USER_ELFS   := $(patsubst %,build/user/%.elf,$(USER_PROGS))

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

build/kernel/user.S.o: $(USER_ELFS)

$(USER_ELFS): build/user/%.elf: user/linker.ld build/user/start.c.o build/user/%.c.o $(LIB_OBJS)
	$(LD) -T user/linker.ld -o $@ $(filter %.o,$^)

-include $(KERNEL_OBJS:.o=.d) $(LIB_OBJS:.o=.d) $(USER_OBJS:.o=.d)
