#include "irq.h"
#include <io.h>

// TS7200 has 64 possible interrupts, where as we only support the first 32
// on versatile PB. Depending on which specific interrupts we need to listen
// to, we may need to support more on the versatile PB later.

#define VWRITE(addr, data) *(volatile unsigned *)(addr) = (data)

#ifdef QEMU
#else
#define VIC1_BASE 0x800b0000
#define VIC2_BASE 0x800c0000

#define ENABLE_OFFSET 0x10
#define SOFTINT_OFFSET 0x18
#define SOFTINT_CLEAR_OFFSET 0x1c
#endif

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
    VWRITE(0x10140010, IRQ_MASK(IRQ_TIMER));
#else
    // writing to VIC1IntEnable & VIC2IntEnable (see ep93xx user manual)
    VWRITE(VIC1_BASE + ENABLE_OFFSET, 0xffffffff);
    VWRITE(VIC2_BASE + ENABLE_OFFSET, 0xffffffff);
#endif
}

void clear_irq(unsigned long long interrupts_c) {
#ifdef QEMU
    VWRITE(0x1014001c, (unsigned) interrupts_c);
#else
    VWRITE(VIC1_BASE + SOFTINT_CLEAR_OFFSET, interrupts_c);
    VWRITE(VIC2_BASE + SOFTINT_CLEAR_OFFSET, interrupts_c);
#endif
}

void set_irq(unsigned long long interrupts) {
#ifdef QEMU
    VWRITE(0x10140018, (unsigned) interrupts);
#else
    VWRITE(VIC1_BASE + SOFTINT_OFFSET, interrupts);
    VWRITE(VIC1_BASE + SOFTINT_OFFSET, interrupts);
#endif
}

unsigned long long get_irq(void) {
#ifdef QEMU
    return (unsigned long long)(*(volatile unsigned*) 0x10140000);
#else
    // TODO: DTRT
    return 0xbeefbeef;
#endif
}
