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

# flags conditional on whether we're building for the real hardware or not
ifeq ($(ENV), qemu)
  CC = arm-none-eabi-gcc
  AS = arm-none-eabi-as
  LD = $(CC)
  # -Wl options are passed through to the linker
  LINKER_SCRIPT = src/qemu.ld
  LDFLAGS = -Wl,-T,$(LINKER_SCRIPT),-Map,$(MAP) -N -static-libgcc --specs=nosys.specs
  CFLAGS += -DQEMU
else
  CC = gcc
  AS = as
  LD = ld
  LINKER_SCRIPT = src/ts7200.ld
  LDFLAGS = -init main -Map $(MAP) -T $(LINKER_SCRIPT) -N -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2
endif

SOURCES=$(shell find $(SRC_DIR) -name '*.c')
COMMON_OBJECTS=$(addsuffix .o, $(basename $(subst $(SRC_DIR),$(BUILD_DIR),$(SOURCES))))

ASM_SOURCES=$(shell find $(SRC_DIR) -name '*.s')
ASM_OBJECTS=$(addsuffix .o, $(basename $(subst $(SRC_DIR),$(BUILD_DIR),$(ASM_SOURCES))))

DIRS=$(sort $(dir $(COMMON_OBJECTS) $(ASM_OBJECTS)))
$(info $(DIRS))

ARM_OBJECTS=$(COMMON_OBJECTS)
ARM_ASSEMBLY=${ARM_OBJECTS:.o=.s}
ARM_DEPENDS=${ARM_OBJECTS:.o=.d}

KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin

# declarations for testing
# TEST_DIR=test
# TEST_BUILD_DIR=build/test
# OBJECTS=$(addprefix $(TEST_BUILD_DIR)/, $(COMMON_OBJECTS))
# DEPENDS=${OBJECTS:.o=.d}
# TEST_OBJECTS=$(addprefix $(TEST_BUILD_DIR)/, priority_test.o)
# TEST_DEPENDS=${TEST_OBJECTS:.o=.d}

# default task
$(KERNEL_BIN): $(KERNEL_ELF)
	arm-none-eabi-objcopy -O binary $< $@

$(KERNEL_ELF): $(ARM_OBJECTS) $(ASM_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(ARM_OBJECTS) $(ASM_OBJECTS) -lgcc

$(KERNEL_ELF): $(LINKER_SCRIPT)

# actual build script for arm parts
# build script for parts that are written by hand in assembly
$(ASM_OBJECTS): $(BUILD_DIR)/%.o : $(SRC_DIR)/%.s
	$(AS) $(ASFLAGS) -o $@ $<

# compile c to assembly, and leave assembly around for inspection
$(ARM_ASSEMBLY): $(BUILD_DIR)/%.s : $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(ARCH_CFLAGS) -S -MD -MT $@ -o $@ $<

# assemble each .s file to a separate object file
$(ARM_OBJECTS): $(BUILD_DIR)/%.o : $(BUILD_DIR)/%.s
	$(AS) $(ASFLAGS) -o $@ $<

# build script for testing
# main source objects, compiled for the local architecture - for testing only
# $(OBJECTS): $(TEST_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
# 	$(CC) $(CFLAGS) -c -MMD -MT ${@:.o=.d} -o $@ $<

# $(TEST_OBJECTS): $(TEST_BUILD_DIR)/%.o : $(TEST_DIR)/%.c
# 	$(CC) $(CFLAGS) -I src -c -MMD -MT ${@:.o=.d} -o $@ $<

# test_runner: $(OBJECTS) $(TEST_OBJECTS)
# 	$(CC) $(CFLAGS) -o $@ $^

# test: test_runner
# 	./$<

install: $(KERNEL_ELF)
	cp $< /u/cs452/tftp/ARM/pgraboud/k.elf

$(ARM_OBJECTS) $(ARM_ASSEMBLY): | $(DIRS)

# $(OBJECTS) $(TEST_OBJECTS): | $(TEST_BUILD_DIR)

$(DIRS):
	@mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

qemu-run: $(KERNEL_BIN)
	echo "Press Ctrl+A x to quit"
	qemu-system-arm -M versatilepb -m 32M -nographic -kernel $(KERNEL_BIN)

qemu-start: $(KERNEL_BIN)
	echo "Starting in suspended mode for debugging... Press Ctrl+A x to quit."
	qemu-system-arm -M versatilepb -m 32M -nographic -s -S -kernel $(KERNEL_BIN)

qemu-debug: $(KERNEL_ELF)
	arm-none-eabi-gdb -ex "target remote localhost:1234" $(KERNEL_ELF)

.PHONY: $(BUILD_DIR) $(TEST_BUILD_DIR) clean qemu-run qemu-debug

-include $(ARM_DEPENDS)
-include $(DEPENDS)
-include $(TEST_DEPENDS)
