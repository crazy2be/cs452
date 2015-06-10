#pragma once

void io_irq_init(void);
void io_irq_handler(int channel);
void io_irq_mask_add(int channel, int is_tx);
