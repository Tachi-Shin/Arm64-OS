
ARCH_PREFIX = aarch64-linux-gnu
CC = $(ARCH_PREFIX)-gcc
LD = $(ARCH_PREFIX)-ld
NM = $(ARCH_PREFIX)-nm
OBJCOPY = $(ARCH_PREFIX)-objcopy

CFLAGS = -O2 -ffreestanding -mgeneral-regs-only -g
ASFLAGS = -g

LDFLAGS = -T test.ld -Map=testout.map


OBJ_DIR=obj

C_SRC := $(wildcard *.c)
S_SRC := $(wildcard *.S)
OBJ_FILES := $(C_SRC:%.c=$(OBJ_DIR)/%.o) \
             $(S_SRC:%.S=$(OBJ_DIR)/%.o)

.phony: all clean

all: arm64os.elf

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o :%.c | $(OBJ_DIR)
	@echo Compiling $<...
	$(CC) -c $(CFLAGS) -MD -MF $@.d -o $@ $<

$(OBJ_DIR)/%.o : %.S | $(OBJ_DIR)
	@echo Compiling $<...
	$(CC) -c  $(ASFLAGS) -MD -MF $@.d -o $@ $<

test.ld: test.ld.src
	@echo Prerocess $<...
	$(CC) -E $(CFGLAGS) -P -x assembler-with-cpp $< > $@

arm64os.elf: $(OBJ_FILES) test.ld
	@echo Linking $@
	$(LD) $(LDFLAGS) -o $@ $(OBJ_FILES)
	$(NM) -n $@ > $(@:elf=sym)
	$(OBJCOPY) -O binary -R .note -R .note.gnu.build-id -R .comment -S $@ $(@:elf=bin)
#	mkimage -a 0x4000000 -e 0x4000000 -A arm64 -T standalone -O u-boot -C none -d arm64os.bin arm64os.img
	mkimage -a 0x4000000 -e 0x4000000 -A arm64 -T standalone -O u-boot -C none -d $(@:elf=bin) $(@:elf=img)
	@echo Done.

$(OBJ_FILES) test.ld arm64os.elf: makefile

clean:
	rm -f $(OBJ_DIR)/* *.elf *.bin *.img *.sym *.map test.ld
