CC = gcc
AS = as
LD = ld

CFLAGS  = -g -fPIC -Wall -Iinclude
ARCH_CFLAGS = -mcpu=arm920t -msoft-float
# -g: include hooks for gdb
# -mcpu=arm920t: generate code for the 920t architecture
# -fpic: emit position-independent code
# -Wall: report all warnings

LDFLAGS = -init main -Map iotest.map -N  -T orex.ld -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2 -L../io/lib

COMMON_OBJECTS=priority.o test.o bwio.o

BUILD_DIR=build
SRC_DIR=src
ARM_OBJECTS=$(addprefix $(BUILD_DIR)/, $(COMMON_OBJECTS))
ARM_DEPENDS=${OBJECTS:.o=.d}

# declarations for testing
TEST_DIR=test
TEST_BUILD_DIR=build/test
OBJECTS=$(addprefix $(TEST_BUILD_DIR)/, $(COMMON_OBJECTS))
DEPENDS=${OBJECTS:.o=.d}
TEST_OBJECTS=$(addprefix $(TEST_BUILD_DIR)/, priority_test.o)
TEST_DEPENDS=${TEST_OBJECTS:.o=.d}

# default task
kernel.elf: $(ARM_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^ -lgcc

$(ARM_OBJECTS): $(BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(ARCH_CFLAGS) -c -MMD -MT ${@:.o=.d} -o $@ $<

# main source objects, compiled for the local architecture - for testing only
$(OBJECTS): $(TEST_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -MMD -MT ${@:.o=.d} -o $@ $<

$(TEST_OBJECTS): $(TEST_BUILD_DIR)/%.o : $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -I src -c -MMD -MT ${@:.o=.d} -o $@ $<

test_runner: $(OBJECTS) $(TEST_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

test: test_runner
	./$<

install: kernel.elf
	cp $< /u/cs452/tftp/ARM/pgraboud/k.elf

$(ARM_OBJECTS): | $(BUILD_DIR)

$(OBJECTS) $(TEST_OBJECTS): | $(TEST_BUILD_DIR)

$(BUILD_DIR) $(TEST_BUILD_DIR):
	@mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: $(BUILD_DIR) $(TEST_BUILD_DIR) clean

-include $(ARM_DEPENDS)
-include $(DEPENDS)
-include $(TEST_DEPENDS)
