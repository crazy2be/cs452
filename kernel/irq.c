#include "irq.h"
#include <io.h>

void setup_irq(void) {
    // TODO: see A.2.6.8 in the arm manual

    // see A.2.5 in the manual
    unsigned cpsr;
    __asm__ ("mrs %0, cpsr" : "=r"(cpsr));
    cpsr &= ~0x80; // deassert the I bit to turn on interrupts
    __asm__ __volatile__ ("msr cpsr, %0" : : "r"(cpsr));

    // enable all interrupts on the interrupt controller
#ifdef QEMU
    // see http://infocenter.arm.com/help/topic/com.arm.doc.dui0224i/DUI0224I_realview_platform_baseboard_for_arm926ej_s_ug.pdf
    // PICIntEnable
    *(volatile unsigned*)(0x10140010) = 0x0000ffff;
#else
    // writing to VICxIntEnable (see ep93xx user manual)
    *(volatile unsigned*)(0x800b0010) = 0xffffffff;
    *(volatile unsigned*)(0x800c0010) = 0xffffffff;
#endif
}

void irq_handler(void) {
    printf("IN IRQ HANDLER" EOL);
    for(;;);
}

void clear_irq(unsigned interrupts_c) {
    volatile unsigned *soft_int;
#ifdef QEMU
    soft_int = (volatile unsigned*) 0x1014001c;
#else
    ASSERT(0);
#endif
    *soft_int = interrupts_c;
}

void set_irq(unsigned interrupts) {
    volatile unsigned *soft_int;
#ifdef QEMU
    soft_int = (volatile unsigned*) 0x10140018;
#else
    ASSERT(0);
#endif
    *soft_int = interrupts;
}
