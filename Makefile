CC      := clang
LD      := ld.lld
OBJCOPY := llvm-objcopy
QEMU    := qemu-system-riscv64

CFLAGS    := --target=riscv64-unknown-elf -mcmodel=medany -ffreestanding -Wall -Wextra -Iinc -MMD -MP
QEMUFLAGS := -machine virt -nographic -kernel build/kernel.elf

USER_PROGS := hello count shell

FIRMWARE_OBJS := $(patsubst %,build/%.o,$(wildcard firmware/*.S))
KERNEL_OBJS   := $(patsubst %,build/%.o,$(wildcard src/*.S src/*.c))
LIB_OBJS      := $(patsubst %,build/%.o,$(wildcard lib/*.c))
USER_OBJS     := $(patsubst %,build/%.o,$(wildcard user/*.c))
USER_ELFS     := $(patsubst %,build/user/%.elf,$(USER_PROGS))
USER_BINS     := $(patsubst %,build/user/%.bin,$(USER_PROGS))

MAKEFLAGS += --no-builtin-rules

.PHONY: all run run-opensbi test clean

all: build/firmware.elf build/kernel.elf

run: all
	$(QEMU) $(QEMUFLAGS) -bios build/firmware.elf

run-opensbi: build/kernel.elf
	$(QEMU) $(QEMUFLAGS) -bios default

test: all
	sh test/smoke.sh firmware $(QEMU) $(QEMUFLAGS) -bios build/firmware.elf
	sh test/smoke.sh opensbi $(QEMU) $(QEMUFLAGS) -bios default

clean:
	rm -rf build

build/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.S.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/firmware.elf: firmware/linker.ld $(FIRMWARE_OBJS)
	$(LD) -T firmware/linker.ld -o $@ $(FIRMWARE_OBJS)

build/kernel.elf: src/linker.ld $(KERNEL_OBJS) $(LIB_OBJS)
	$(LD) -T src/linker.ld -o $@ $(KERNEL_OBJS) $(LIB_OBJS)

build/src/user.S.o: $(USER_BINS)

$(USER_ELFS): build/user/%.elf: user/linker.ld build/user/start.c.o build/user/%.c.o $(LIB_OBJS)
	$(LD) -T user/linker.ld -o $@ $(filter %.o,$^)

build/user/%.bin: build/user/%.elf
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

-include $(FIRMWARE_OBJS:.o=.d) $(KERNEL_OBJS:.o=.d) $(LIB_OBJS:.o=.d) $(USER_OBJS:.o=.d)
