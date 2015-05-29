#include "irq.h"
#include <io.h>

#define VWRITE(addr, data) *(volatile unsigned *)(addr) = (data)

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
    VWRITE(0x10140010, 0x0000ffff);
#else
    // writing to VICxIntEnable (see ep93xx user manual)
    VWRITE(0x800b0010, 0xffffffff);
    VWRITE(0x800c0010, 0xffffffff);
#endif
}

void irq_handler(void) {
    printf("IN IRQ HANDLER" EOL);
    for(;;);
}

void clear_irq(unsigned interrupts_c) {
#ifdef QEMU
    VWRITE(0x1014001c, interrupts_c);
    // DEBUG: disable interrupts after clearing
    // NOTE: if we disable interrupts here, the problem doesn't go away
    // this leads me to believe that whatever state gets screwed up isn't caused by further interrupts
    // We also know that this does actually disable interrupts, since if we comment this
    // out, we can send in further interrupts
    VWRITE(0x10140014, 0x0000ffff);
#else
    ASSERT(0);
#endif
}

void set_irq(unsigned interrupts) {
#ifdef QEMU
    VWRITE(0x10140018, interrupts);
#else
    ASSERT(0);
#endif
}
