#pragma once

#include "../task_descriptor.h"

void io_irq_init(void);
void io_irq_handler(int channel);
int io_irq_check_for_pending_tx(int channel, struct task_descriptor *td);
int io_irq_check_for_pending_rx(int channel, struct task_descriptor *td);
