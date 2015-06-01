MAKEFILE_NAME = ${firstword ${MAKEFILE_LIST}} # makefile name

BUILD_DIR=build
KERNEL_SRC_DIR=kernel
GEN_SRC_DIR=gen
TEST_SRC_DIR=test
USER_SRC_DIR=user

BENCHMARK_FLAGS = -DBENCHMARK_CACHE -DBENCHMARK_SEND_FIRST -DBENCHMARK_MSG_SIZE=64

CFLAGS  = -g -fPIC -Wall -Werror -Iinclude -std=c99 -O2 $(BENCHMARK_FLAGS)
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
LINKER_SCRIPT = qemu.ld
LDFLAGS = -Wl,-T,$(LINKER_SCRIPT),-Map,$(MAP) -N -static-libgcc --specs=nosys.specs
CFLAGS += -DQEMU
default: $(KERNEL_BIN)
else
export PATH := /u/wbcowan/gnuarm-4.0.2/libexec/gcc/arm-elf/4.0.2:/u/wbcowan/gnuarm-4.0.2/arm-elf/bin:$(PATH)
CC = gcc
AS = as
LD = ld
LINKER_SCRIPT = ts7200.ld
LDFLAGS = -init main -Map $(MAP) -T $(LINKER_SCRIPT) -N -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2
default: install
endif

# macro to turn source names into object names
objectify=$(addprefix $(BUILD_DIR)/, $(addsuffix .o, $(basename $(1))))

# find sources for each subproject independantly,
# since it's not easy to split them back up again with make
KERNEL_SOURCES=$(shell find $(KERNEL_SRC_DIR) -name *.c)
USER_SOURCES=$(shell find $(USER_SRC_DIR) -name *.c)
TEST_SOURCES=$(shell find $(TEST_SRC_DIR) -name *.c)

KERNEL_ASM_SOURCES=$(shell find $(KERNEL_SRC_DIR) -name *.s) $(GEN_SRC_DIR)/syscalls.s
USER_ASM_SOURCES=$(shell find $(USER_SRC_DIR) -name *.s)
TEST_ASM_SOURCES=$(shell find $(TEST_SRC_DIR) -name *.s)

# definitions for compiling C source code
SOURCES=$(KERNEL_SOURCES) $(USER_SOURCES) $(TEST_SOURCES)
OBJECTS=$(call objectify, $(SOURCES))
GENERATED_ASSEMBLY=${OBJECTS:.o=.s}
DEPENDS=${OBJECTS:.o=.d}

# definitions for assembling handwritten assembly
ASM_SOURCES=$(KERNEL_ASM_SOURCES) $(USER_ASM_SOURCES) $(TEST_ASM_SOURCES)
ASM_OBJECTS=$(call objectify, $(ASM_SOURCES))

DIRS=$(sort $(dir $(OBJECTS) $(ASM_OBJECTS)))

# definitions used for linker recipe
KERNEL_COMMON_OBJECTS = $(call objectify, $(KERNEL_SOURCES) $(KERNEL_ASM_SOURCES))
USER_COMMON_OBJECTS = $(call objectify, $(USER_SOURCES) $(USER_ASM_SOURCES))
TEST_COMMON_OBJECTS = $(call objectify, $(TEST_SOURCES) $(TEST_ASM_SOURCES))
GENERATED_SOURCES = $(GEN_SRC_DIR)/syscalls.s $(GEN_SRC_DIR)/syscalls.h


$(KERNEL_BIN) $(TEST_BIN): %.bin : %.elf
	arm-none-eabi-objcopy -O binary $< $@

$(TEST_ELF): $(KERNEL_COMMON_OBJECTS) $(TEST_COMMON_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_COMMON_OBJECTS) $(TEST_COMMON_OBJECTS) -lgcc

$(KERNEL_ELF): $(KERNEL_COMMON_OBJECTS) $(USER_COMMON_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_COMMON_OBJECTS) $(USER_COMMON_OBJECTS) -lgcc

$(KERNEL_ELF) $(TEST_ELF): $(LINKER_SCRIPT)

# actual build script for arm parts

# assemble hand-written assembly
$(ASM_OBJECTS): $(BUILD_DIR)/%.o : %.s
	$(AS) $(ASFLAGS) -o $@ $<

# compile c to assembly, and leave assembly around for inspection
$(GENERATED_ASSEMBLY): $(BUILD_DIR)/%.s : %.c
	$(CC) $(CFLAGS) $(ARCH_CFLAGS) -S -MD -MT $@ -o $@ $<

# assemble the generated assembly
$(C_OBJECTS): $(BUILD_DIR)/%.o : $(BUILD_DIR)/%.s
	$(AS) $(ASFLAGS) -o $@ $<

# generate build directories before starting build
$(C_OBJECTS) $(GENERATED_ASSEMBLY): | $(DIRS)

$(GENERATED_ASSEMBLY): $(GENERATED_SOURCES)

$(GENERATED_SOURCES): kernel/syscall.py
	mkdir -p $(GEN_SRC_DIR)
	python $<

$(DIRS):
	@mkdir -p $@

# regenerate everything after the makefile changes
$(ASM_OBJECTS) $(GENERATED_ASSEMBLY): $(MAKEFILE_NAME)

clean:
	rm -rf $(BUILD_DIR) $(GEN_SRC_DIR)

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

qemu-start-test: $(TEST_BIN)
	@echo "Starting in suspended mode for debugging... Press Ctrl+A x to quit."
	qemu-system-arm -M versatilepb -m 32M -nographic -s -S -kernel $(TEST_BIN)

qemu-debug-test: $(TEST_ELF)
	arm-none-eabi-gdb -ex "target remote localhost:1234" $(TEST_ELF)

sync:
	rsync -avzd . uw:cs452-kernel/
	ssh uw "bash -c 'cd cs452-kernel && make clean && make ENV=ts7200 install'"

TEST_OUTPUT=test_output
TEST_EXPECTED=test_expected

test-run: $(TEST_BIN)
	@echo "Press Ctrl+A x to quit"
	qemu-system-arm -M versatilepb -m 32M -nographic -kernel $(TEST_BIN)

test-start: $(TEST_BIN)
	@echo "Starting tests in suspended mode for debugging... Press Ctrl+A x to quit."
	qemu-system-arm -M versatilepb -m 32M -nographic -s -S -kernel $(TEST_BIN)

test-debug: $(TEST_ELF)
	arm-none-eabi-gdb -ex "target remote localhost:1234" $(TEST_ELF)

test: $(TEST_BIN)
	./qemu_capture $(TEST_BIN) $(TEST_ELF) $(TEST_OUTPUT)
	diff -y $(TEST_EXPECTED) $(TEST_OUTPUT)

.PHONY: clean qemu-run qemu-debug default install

-include $(DEPENDS)
