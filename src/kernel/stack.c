#include "stack.h"
// This is in a separate file so that we can put it outside the usual BSS
// segment with the linker script.
// We zero out the BSS during bootup, but it takes to long to zero out all
// the user stacks, since that's a lot of memory.
// Putting the stacks outside of BSS prevents this from happening.
char stacks[NUM_TD][USER_STACK_SIZE];
