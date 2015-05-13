#include "bwio.h"
static unsigned test1 = 8;
const unsigned test3 = 8;

int main (int argc, char *argv[]) {
    int test2 = 7;
    bwsetfifo(COM2, 1);
label:
    bwsetspeed(COM2, 115200);
    bwprintf(COM2, "Hello world %x (%x %x), (%x %x), (%x %x)\n\r", &&label, &test1, test1, &test2, test2, &test3, test3);
    return test2;
}
