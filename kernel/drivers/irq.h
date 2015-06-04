#pragma once

void irq_setup(void);
unsigned long long irq_get_interrupt(void);
void irq_cleanup(void);

// each of these is a bit index, so the mask for IRQ_X is (0x1 << IRQ_X)
#define IRQ_MASK(x) (0x1 << (x))
#ifdef QEMU
#define IRQ_TIMER 4
#else
#define IRQ_TIMER 51
#endif
