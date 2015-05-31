#pragma once

void setup_irq(void);
void set_irq(unsigned long long status);
void clear_irq(unsigned long long interrupts_c);
unsigned long long get_irq(void);

// each of these is a bit index, so the mask for IRQ_X is (0x1 << IRQ_X)
#define IRQ_MASK(x) (0x1 << (x))
#ifdef QEMU
#define IRQ_TIMER 4
#endif
