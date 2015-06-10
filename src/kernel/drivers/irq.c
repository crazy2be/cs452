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

#define STATUS_OFFSET 0x0
#define ENABLE_OFFSET 0x10
#define SOFTINT_OFFSET 0x18
#define SOFTINT_CLEAR_OFFSET 0x1c
#endif

void dump_irq(void) {
#ifdef QEMU
#else
	volatile unsigned* data = (unsigned*) VIC1_BASE;
	for (unsigned i = 0; i < 32; i++) {
		printf("%x ", data[i]);
	}
	printf(EOL);
#endif
}

void irq_setup(void) {
	// TODO: see A.2.6.8 in the arm manual

	// see A.2.5 in the manual
	unsigned cpsr;
	__asm__ ("mrs %0, cpsr" : "=r"(cpsr));
	cpsr |= 0xc0; // assert the I & F bits to disable all interrupts (normal & fast)
	__asm__ __volatile__ ("msr cpsr, %0" : : "r"(cpsr));

	// clear any soft interrupts
#ifdef QEMU
	VWRITE(0x1014001c, 0xffffffff);
#else
	VWRITE(VIC1_BASE + SOFTINT_CLEAR_OFFSET, 0xffffffff);
	VWRITE(VIC2_BASE + SOFTINT_CLEAR_OFFSET, 0xffffffff);
#endif

	// enable all interrupts on the interrupt controller
#ifdef QEMU
	// see http://infocenter.arm.com/help/topic/com.arm.doc.dui0224i/DUI0224I_realview_platform_baseboard_for_arm926ej_s_ug.pdf
	// PICIntEnable
	VWRITE(0x10140010, IRQ_MASK(IRQ_TIMER) | IRQ_MASK(IRQ_COM1));
#else
	// writing to VIC1IntEnable & VIC2IntEnable (see ep93xx user manual)
	VWRITE(VIC1_BASE + ENABLE_OFFSET, 0x0);
	VWRITE(VIC2_BASE + ENABLE_OFFSET, IRQ_MASK(IRQ_TIMER - 32) | IRQ_MASK(IRQ_COM1 - 32));
#endif
}

// intended to be called after the shutdown of the kernel, so as not to mess with redboot
void irq_cleanup(void) {
#ifndef QEMU
    VWRITE(VIC1_BASE + ENABLE_OFFSET, 0);
    VWRITE(VIC2_BASE + ENABLE_OFFSET, 0);
#else
    VWRITE(0x10140010, 0);
#endif
    __asm__ __volatile__ ("msr cpsr_c, #0x13");
}

unsigned long long irq_get_interrupt(void) {
#ifdef QEMU
	return (unsigned long long)(*(volatile unsigned*) 0x10140000);
#else
	// TODO: DTRT
	unsigned lo = *(volatile unsigned*)(VIC1_BASE + STATUS_OFFSET);
	unsigned hi = *(volatile unsigned*)(VIC2_BASE + STATUS_OFFSET);
	return (((unsigned long long)hi) << 32) | ((unsigned long long) lo);
#endif
}
