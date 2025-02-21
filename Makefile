K = kernel
U = user

SRCS = $(wildcard $(K)/boot/*.S) $(wildcard $(K)/boot/*.c)
SRCS += $(wildcard $(K)/dev/*.S) $(wildcard $(K)/dev/*.c)
SRCS += $(wildcard $(K)/fs/*.S) $(wildcard $(K)/fs/*.c)
SRCS += $(wildcard $(K)/lib/*.S) $(wildcard $(K)/lib/*.c)
SRCS += $(wildcard $(K)/mm/*.S) $(wildcard $(K)/mm/*.c)
SRCS += $(wildcard $(K)/sched/*.S) $(wildcard $(K)/sched/*.c)
SRCS += $(wildcard $(K)/syscall/*.S) $(wildcard $(K)/syscall/*.c)
SRCS += $(wildcard $(K)/trap/*.S) $(wildcard $(K)/trap/*.c)
SRCS += $(wildcard $(K)/*.S) $(wildcard $(K)/*.c)
OBJS = $(subst .c,.o,$(subst .S,.o,$(SRCS)))

UPROGS = \
$(U)/_cat \
$(U)/_cp \
$(U)/_echo \
$(U)/_grep \
$(U)/_init \
$(U)/_kill \
$(U)/_ln \
$(U)/_ls \
$(U)/_mkdir \
$(U)/_rm \
$(U)/_sh

CC = riscv64-unknown-elf-gcc
LD = riscv64-unknown-elf-ld
OBJCOPY = riscv64-unknown-elf-objcopy
OBJDUMP = riscv64-unknown-elf-objdump

CPPFLAGS = -Iinclude -MD

CFLAGS = -Wall -Werror -O
CFLAGS += -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096

QEMU = qemu-system-riscv64
QEMU_OPTS = -machine virt -kernel kernel.elf
QEMU_OPTS += -m 128M -smp 3 -nographic
QEMU_OPTS += -bios default
QEMU_OPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMU_OPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

.PHONY: all clean qemu qemu-gdb

all: kernel.elf

clean:
	rm -f ./*/*.o ./*/*.d ./*/*.out
	rm -f ./*/*/*.o ./*/*/*.d ./*/*/*.out
	rm -f kernel.elf $(UPROGS) fs.img tools/mkfs $(U)/initcode \
	$(U)/initcode_c_arr.h

qemu: kernel.elf fs.img
	$(QEMU) $(QEMU_OPTS)

qemu-gdb: kernel.elf fs.img
	$(QEMU) $(QEMU_OPTS) -S -s

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -T linker/kernel.ld -o $@ $^

fs.img: tools/mkfs README $(UPROGS)
	tools/mkfs fs.img README $(UPROGS)

tools/mkfs: tools/mkfs.c
	gcc -Wall -Werror -Iinclude -o tools/mkfs tools/mkfs.c

$(U)/initcode: $(U)/initcode.S
	$(CC) $(CPPFLAGS) $(CFLAGS) -march=rv64g -nostdinc -c -o $(U)/initcode.o $<
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $(U)/initcode.out $(U)/initcode.o
	$(OBJCOPY) -S -O binary $(U)/initcode.out $@
	xxd -i $(U)/initcode > $(U)/initcode_c_arr.h

$(U)/_%: $(U)/%.o $(U)/usys.o $(U)/ulib.o
	$(LD) $(LDFLAGS) -T linker/user.ld -o $@ $^
