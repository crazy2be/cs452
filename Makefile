MAKEFILE_NAME = ${firstword ${MAKEFILE_LIST}} # makefile name

BUILD_DIR=build
SRC_DIR=src

CFLAGS  = -g -fPIC -Wall -Werror -Iinclude -std=c99 -O2
ARCH_CFLAGS = -mcpu=arm920t -msoft-float
# -g: include hooks for gdb
# -mcpu=arm920t: generate code for the 920t architecture
# -fpic: emit position-independent code
# -Wall: report all warnings

ASFLAGS = -mcpu=arm920t -mapcs-32

MAP = ${BUILD_DIR}/kernel.map
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin

TEST_ELF = $(BUILD_DIR)/test_kernel.elf
TEST_BIN = $(BUILD_DIR)/test_kernel.bin

# try to autodetect environment
ifeq ($(ENV),)
ifeq ($(shell which arm-none-eabi-gcc), )
ENV = arm920t
else
ENV = qemu
endif
else
endif

# flags conditional on whether we're building for the real hardware or not
# also specify default targets depending on which version we're using
ifeq ($(ENV), qemu)
CC = arm-none-eabi-gcc
AS = arm-none-eabi-as
LD = $(CC)
# -Wl options are passed through to the linker
LINKER_SCRIPT = src/qemu.ld
LDFLAGS = -Wl,-T,$(LINKER_SCRIPT),-Map,$(MAP) -N -static-libgcc --specs=nosys.specs
CFLAGS += -DQEMU
default: $(KERNEL_BIN)
else
export PATH := /u/wbcowan/gnuarm-4.0.2/libexec/gcc/arm-elf/4.0.2:/u/wbcowan/gnuarm-4.0.2/arm-elf/bin:$(PATH)
CC = gcc
AS = as
LD = ld
LINKER_SCRIPT = src/ts7200.ld
LDFLAGS = -init main -Map $(MAP) -T $(LINKER_SCRIPT) -N -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2
default: install
endif

# variables describing objects from C code
SOURCES=$(shell find $(SRC_DIR) -name '*.c')
OBJECTS=$(addsuffix .o, $(basename $(subst $(SRC_DIR),$(BUILD_DIR),$(SOURCES))))
GENERATED_ASSEMBLY=${OBJECTS:.o=.s}
DEPENDS=${OBJECTS:.o=.d}

# variables describing objects from handwritten assembly
ASM_SOURCES=$(shell find $(SRC_DIR) -name '*.s')
ASM_OBJECTS=$(addsuffix .o, $(basename $(subst $(SRC_DIR),$(BUILD_DIR),$(ASM_SOURCES))))

DIRS=$(sort $(dir $(OBJECTS) $(ASM_OBJECTS)))

# objects shared by both test and real kernels
KERNEL_INIT = $(BUILD_DIR)/init_task.o
TEST_INIT = $(BUILD_DIR)/init_task_test.o
COMMON_OBJECTS = $(filter-out $(KERNEL_INIT) $(TEST_INIT), $(OBJECTS) $(ASM_OBJECTS))

$(KERNEL_BIN) $(TEST_BIN): %.bin : %.elf
	arm-none-eabi-objcopy -O binary $< $@

$(TEST_ELF): $(COMMON_OBJECTS) $(TEST_INIT)
	$(LD) $(LDFLAGS) -o $@ $(COMMON_OBJECTS) $(TEST_INIT) -lgcc

$(KERNEL_ELF): $(COMMON_OBJECTS) $(KERNEL_INIT)
	$(LD) $(LDFLAGS) -o $@ $(COMMON_OBJECTS) $(KERNEL_INIT) -lgcc

$(KERNEL_ELF) $(TEST_ELF): $(LINKER_SCRIPT)

# actual build script for arm parts

# assemble hand-written assembly
$(ASM_OBJECTS): $(BUILD_DIR)/%.o : $(SRC_DIR)/%.s
	$(AS) $(ASFLAGS) -o $@ $<

# compile c to assembly, and leave assembly around for inspection
$(GENERATED_ASSEMBLY): $(BUILD_DIR)/%.s : $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(ARCH_CFLAGS) -S -MD -MT $@ -o $@ $<

# assemble the generated assembly
$(C_OBJECTS): $(BUILD_DIR)/%.o : $(BUILD_DIR)/%.s
	$(AS) $(ASFLAGS) -o $@ $<

# generate build directories before starting build
$(C_OBJECTS) $(GENERATED_ASSEMBLY): | $(DIRS)

$(DIRS):
	@mkdir -p $@

# regenerate everything after the makefile changes
$(ASM_OBJECTS) $(GENERATED_ASSEMBLY): $(MAKEFILE_NAME)

clean:
	rm -rf $(BUILD_DIR)

ELF_DESTINATION = /u/cs452/tftp/ARM/$(USER)/k.elf
install: $(KERNEL_ELF)
	cp $< $(ELF_DESTINATION)
	chmod a+r $(ELF_DESTINATION)

# scripts for convenience

qemu-run: $(KERNEL_BIN)
	@echo "Press Ctrl+A x to quit"
	qemu-system-arm -M versatilepb -m 32M -nographic -kernel $(KERNEL_BIN)

qemu-start: $(KERNEL_BIN)
	@echo "Starting in suspended mode for debugging... Press Ctrl+A x to quit."
	qemu-system-arm -M versatilepb -m 32M -nographic -s -S -kernel $(KERNEL_BIN)

qemu-debug: $(KERNEL_ELF)
	arm-none-eabi-gdb -ex "target remote localhost:1234" $(KERNEL_ELF)

sync:
	rsync -avzd . uw:cs452-kernel/
	ssh uw "bash -c 'cd cs452-kernel && make clean && make ENV=ts7200 install'"

TEST_OUTPUT=test_output
TEST_EXPECTED=test_expected

test: $(TEST_BIN)
	./qemu_capture $(TEST_BIN) $(TEST_ELF) $(TEST_OUTPUT)
	diff -y $(TEST_EXPECTED) $(TEST_OUTPUT)

.PHONY: clean qemu-run qemu-debug default install

-include $(DEPENDS)
