#include <kernel.h>
#include <io.h>
#include <assert.h>
#include "nameserver.h"
#include "clockserver.h"
#include "../kernel/util.h"
#include "../kernel/drivers/timer.h"

void init_task(void) {
	unsigned t_next = 1000000;
	for (;;) {
		unsigned t = debug_timer_useconds();
		if (t >= t_next) {
			t_next += 1000000;
			printf("%d useconds passed" EOL, t);
		}
	}
	/* *(volatile unsigned*) 0x10140018 = 0xdeadbeef; */
	/* printf("After interrupt generated" EOL); */
	/* *(volatile unsigned*) 0x10140018 = 0xdeadbeef; */
	/* printf("After interrupt generated" EOL); */
}

void await_task(void) {
	printf("About to await" EOL);
	unsigned start = debug_timer_useconds();
	delay(100);
	unsigned end = debug_timer_useconds();
	printf("Finished await: %d" EOL, end - start);
}

void await_init_task(void) {
	create(PRIORITY_MAX + 2, nameserver);
	create(PRIORITY_MAX + 1, clockserver);

	for (int i = 0; i < 10; i++) {
		create(PRIORITY_MAX + 3, await_task);
	}
	printf("Created 10" EOL);
}

#include "benchmark.h"
int main(int argc, char *argv[]) {
	boot(benchmark, 0);
	/* boot(await_init_task, 0); */
	/* boot(init_task, 0); */
}
