MAKEFILE_NAME = ${firstword ${MAKEFILE_LIST}} # makefile name

BUILD_DIR=build
SRC_DIR=src
KERNEL_SRC_DIR=$(SRC_DIR)/kernel
GEN_SRC_DIR=$(SRC_DIR)/gen
TEST_SRC_DIR=$(SRC_DIR)/test
USER_SRC_DIR=$(SRC_DIR)/user
LIB_SRC_DIR=$(SRC_DIR)/lib

BENCHMARK_FLAGS = -DBENCHMARK_CACHE -DBENCHMARK_SEND_FIRST -DBENCHMARK_MSG_SIZE=64

STACK_SEED := $(shell date +%N)
CFLAGS  = -g -fPIC -Wall -Werror -I$(SRC_DIR)/lib -std=c99 -O2 \
	$(BENCHMARK_FLAGS) \
	-fno-builtin-puts -fno-builtin-fputs -fno-builtin-fputc -fno-builtin-putc \
	-fverbose-asm -DSTACK_SEED=$(STACK_SEED)
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

CFLAGS += -DTRACKA

ifeq ($(TYPE),c)
CFLAGS += -DCALIBRATE
endif

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
else ifeq ($(ENV), cow)
# TODO: We probably want to remove this once we know that the proper toolchain is working.
export PATH := /u/wbcowan/gnuarm-4.0.2/libexec/gcc/arm-elf/4.0.2:/u/wbcowan/gnuarm-4.0.2/arm-elf/bin:$(PATH)
CC = gcc
AS = as
LD = ld
LINKER_SCRIPT = ts7200.ld
LDFLAGS = -init main -Map $(MAP) -T $(LINKER_SCRIPT) -N -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2
else
export PATH := /u1/jmcgirr/arm-toolchain/current/bin:$(PATH)
CC = arm-none-eabi-gcc
AS = arm-none-eabi-as
LD = $(CC)
LINKER_SCRIPT = ts7200.ld
LDFLAGS = -Wl,-T,$(LINKER_SCRIPT),-Map,$(MAP) -N -static-libgcc --specs=nosys.specs -L/u1/jmcgirr/arm-toolchain/current/lib/gcc/arm-none-eabi/4.9.3
default: install
endif

# macro to turn source names into object names
objectify=$(subst $(SRC_DIR)/, $(BUILD_DIR)/, $(addsuffix .o, $(basename $(1))))

GENERATED_SOURCES = $(GEN_SRC_DIR)/syscalls.s $(GEN_SRC_DIR)/syscalls.h

# find sources for each subproject independantly,
# since it's not easy to split them back up again with make
KERNEL_SOURCES:=$(shell find $(KERNEL_SRC_DIR) -name *.c)
USER_SOURCES:=$(shell find $(USER_SRC_DIR) -name *.c)
TEST_SOURCES:=$(shell find $(TEST_SRC_DIR) -name *.c)
LIB_SOURCES:=$(shell find $(LIB_SRC_DIR) -name *.c)

KERNEL_ASM_SOURCES:=$(shell find $(KERNEL_SRC_DIR) -name *.s) $(GEN_SRC_DIR)/syscalls.s
USER_ASM_SOURCES:=$(shell find $(USER_SRC_DIR) -name *.s)
TEST_ASM_SOURCES:=$(shell find $(TEST_SRC_DIR) -name *.s)
LIB_ASM_SOURCES:=$(shell find $(LIB_SRC_DIR) -name *.s)

# definitions for compiling C source code
SOURCES=$(KERNEL_SOURCES) $(USER_SOURCES) $(TEST_SOURCES) $(LIB_SOURCES)
OBJECTS=$(call objectify, $(SOURCES))
GENERATED_ASSEMBLY=${OBJECTS:.o=.s}
DEPENDS=${OBJECTS:.o=.d}

# definitions for assembling handwritten assembly
ASM_SOURCES=$(KERNEL_ASM_SOURCES) $(USER_ASM_SOURCES) $(TEST_ASM_SOURCES) $(LIB_ASM_SOURCES)
ASM_OBJECTS=$(call objectify, $(ASM_SOURCES))

DIRS=$(sort $(dir $(OBJECTS) $(ASM_OBJECTS)))

# definitions used for linker recipe
# "common" objects refers the fact that these come from both C and ASM files
KERNEL_COMMON_OBJECTS = $(call objectify, $(KERNEL_SOURCES) $(KERNEL_ASM_SOURCES))
USER_COMMON_OBJECTS = $(call objectify, $(USER_SOURCES) $(USER_ASM_SOURCES))
TEST_COMMON_OBJECTS = $(call objectify, $(TEST_SOURCES) $(TEST_ASM_SOURCES))
LIB_COMMON_OBJECTS = $(call objectify, $(LIB_SOURCES) $(LIB_ASM_SOURCES))

TEST_ALL_OBJECTS = $(KERNEL_COMMON_OBJECTS) $(LIB_COMMON_OBJECTS) $(TEST_COMMON_OBJECTS) $(filter-out $(BUILD_DIR)/user/main.o, $(USER_COMMON_OBJECTS))
USER_ALL_OBJECTS = $(KERNEL_COMMON_OBJECTS) $(LIB_COMMON_OBJECTS) $(USER_COMMON_OBJECTS)

# unity build definitions
UNITY_SOURCE = $(BUILD_DIR)/unity.c
UNITY_OBJ = $(UNITY_SOURCE:.c=.o)
UNITY_DEPEND = $(UNITY_SOURCE:.c=.d)

# whenever we change these flags, we want to rebuild
FLAGS = $(ENV) $(TYPE)
# we write the last flags we had to the file, and make that file a dependency
# of everything else. whenever we write to the file, it will cause everything else
# to need to be rebuilt
FLAGFILE = $(BUILD_DIR)/flags
$(shell mkdir -p $(DIRS))

ifeq ($(wildcard $(FLAGFILE)), )
# create the file if it doesn't exist (which will cause a rebuild)
# don't use the file command, since it apparently doesn't work in the student environment
$(shell echo $(FLAGS) > $(FLAGFILE))
else
ifneq ($(strip $(shell cat $(FLAGFILE))), $(strip $(FLAGS))) # check if the file doesn't match
# if the contents of the file doesn't match our current flags, write to the
# file, which will cause a rebuild
$(shell echo $(FLAGS) > $(FLAGFILE))
endif
endif

$(KERNEL_BIN) $(TEST_BIN): %.bin : %.elf
	arm-none-eabi-objcopy -O binary $< $@

$(TEST_ELF): $(TEST_ALL_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(TEST_ALL_OBJECTS) -lgcc

# optionally perform a unity build for better optimization
ifdef UNITY
$(KERNEL_ELF): $(UNITY_OBJ) $(ASM_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(UNITY_OBJ) $(ASM_OBJECTS) -lgcc
else
$(KERNEL_ELF): $(USER_ALL_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(USER_ALL_OBJECTS) -lgcc
endif

$(KERNEL_ELF) $(TEST_ELF): $(LINKER_SCRIPT)

# actual build script for arm parts

# assemble hand-written assembly
$(ASM_OBJECTS): $(BUILD_DIR)/%.o : $(SRC_DIR)/%.s
	$(AS) $(ASFLAGS) -o $@ $<

# compile c to assembly, and leave assembly around for inspection
$(GENERATED_ASSEMBLY): $(BUILD_DIR)/%.s : $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(ARCH_CFLAGS) -S -MD -MT $@ -o $@ $<

# assemble the generated assembly
$(OBJECTS): $(BUILD_DIR)/%.o : $(BUILD_DIR)/%.s
	$(AS) $(ASFLAGS) -o $@ $<

$(GENERATED_ASSEMBLY): $(GENERATED_SOURCES)

$(GENERATED_SOURCES): $(KERNEL_SRC_DIR)/syscall.py
	mkdir -p $(GEN_SRC_DIR)
	python $< $(GEN_SRC_DIR)

# regenerate everything after the makefile or flags change
$(GENERATED_ASSEMBLY) $(ASM_OBJECTS) $(UNITY_SOURCE): $(MAKEFILE_NAME) $(FLAGFILE)

# this generates a "unity compile" for the main kernel/user program
# this is admittedly pretty gross, but helps the compiler optimize things better
$(UNITY_SOURCE): $(MAKEFILE_NAME) $(KERNEL_SOURCES) $(USER_SOURCES) $(LIB_SOURCES)
	./unity.sh $(KERNEL_SOURCES) $(USER_SOURCES) $(LIB_SOURCES) > $@

$(UNITY_OBJ): $(UNITY_SOURCE) $(GENERATED_SOURCES)
	$(CC) $(CFLAGS) $(ARCH_CFLAGS) -c -MD -MT $@ -o $@ $<

-include $(UNITY_DEPEND)

clean:
	rm -rf $(BUILD_DIR) $(GEN_SRC_DIR)

ELF_DESTINATION = /u/cs452/tftp/ARM/$(USER)/$(TYPE).elf
install: $(KERNEL_ELF)
	cp $< $(ELF_DESTINATION)
	chmod a+r $(ELF_DESTINATION)

# scripts for convenience
# TODO: there's a lot of different qemu-system-arm invocations - we should DRY this up
qemu-run: $(KERNEL_BIN)
	./scripts/qemu run $<

qemu-start: $(KERNEL_BIN)
	./scripts/qemu start $<

qemu-debug: $(KERNEL_ELF)
	./scripts/qemu debug $<

qemu-fast-test: $(TEST_BIN)
	./scripts/qemu print $<

qemu-run-test: $(TEST_BIN)
	./scripts/qemu console $<

qemu-start-test: $(TEST_BIN)
	./scripts/qemu start $<

qemu-debug-test: $(TEST_ELF)
	./scripts/qemu debug $<

sync-clean:
	ssh uw "rm -rf cs452-kernel"

sync:
	rsync -avzd --exclude /build --exclude vid --exclude vid2 --exclude vid3 --exclude /.git --exclude /writeup . uw:cs452-kernel/

rt: sync
	ssh uw "bash -c 'cd cs452-kernel && make -j4 ENV=arm920t TYPE=t install'"

rc: sync
	ssh uw "bash -c 'cd cs452-kernel && make -j4 ENV=arm920t TYPE=c install'"

all: $(KERNEL_ELF)

TEST_COM1_OUTPUT=scripts/test_com1_output
TEST_COM2_OUTPUT=scripts/test_com2_output
TEST_COM1_EXPECTED=scripts/test_com1_expected
TEST_COM2_EXPECTED=scripts/test_com2_expected

test: $(TEST_BIN)
	./scripts/qemu_capture $(TEST_BIN) $(TEST_ELF) $(TEST_COM1_OUTPUT) $(TEST_COM2_OUTPUT)
	diff --text -y $(TEST_COM1_EXPECTED) $(TEST_COM1_OUTPUT)
	diff --text -y $(TEST_COM2_EXPECTED) $(TEST_COM2_OUTPUT)

format:
	astyle -R --style=java --keep-one-line-statements --suffix=none \
		--indent=tab 'src/*.c' 'src/*.h'

.PHONY: clean qemu-run qemu-debug default install format

-include $(DEPENDS)
