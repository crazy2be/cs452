#pragma once

#include "../task_descriptor.h"

void io_irq_init(void);
void io_irq_handler(int channel);
void io_irq_mask_add(int channel, int is_write);
