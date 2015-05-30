#pragma once

void setup_irq(void);
void set_irq(unsigned status);
void clear_irq(unsigned interrupts_c);
unsigned get_irq(void);
