#pragma once

void irq_setup(void);
unsigned long long irq_get_interrupt(void);
void irq_cleanup(void);

// each of these is a bit index, so the mask for IRQ_X is (0x1 << IRQ_X)
#define IRQ_MASK(x) (0x1 << (x))
#define IRQ_TEST(x, lo, hi) (((x) <= 32) ? (lo) & IRQ_MASK(x) : (hi) & IRQ_MASK(x - 32))
#ifdef QEMU
#define IRQ_TIMER 4
#else
#define IRQ_TIMER 51
#define IRQ_COM1 52
#define IRQ_COM2 54
#endif
