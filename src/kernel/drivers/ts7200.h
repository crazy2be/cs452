#pragma once

/*
 * ts7200.h - definitions describing the ts7200 peripheral registers
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */

#define	TIMER1_BASE	0x80810000
#define	TIMER2_BASE	0x80810020
#define	TIMER3_BASE	0x80810080

#define	LDR_OFFSET	0x00000000	// 16/32 bits, RW
#define	VAL_OFFSET	0x00000004	// 16/32 bits, RO
#define CRTL_OFFSET	0x00000008	// 3 bits, RW
#define	ENABLE_MASK	0x00000080
#define	MODE_MASK	0x00000040
#define	CLKSEL_MASK	0x00000008
#define CLR_OFFSET	0x0000000c	// no data, WO


#define LED_ADDRESS	0x80840020
#define LED_NONE	0x0
#define LED_GREEN	0x1
#define LED_RED		0x2
#define LED_BOTH	0x3

#define IRDA_BASE	0x808b0000
#ifdef QEMU
#define UART0_BASE 0x101F1000
#define UART1_BASE 0x101F2000
#else
#define UART0_BASE	0x808c0000
#define UART1_BASE	0x808d0000
// COM3 / UART1 is somewhere, we just have no idea how to hook it up. Take a
// look at http://wiki.embeddedarm.com/wiki/TS-ENC720, which is the same
// internal board we have. It looks like it may be attached to the DIO header
// (although which pins is anybody's guess). There is something which looks
// like a schematic of this, but I can't make sense of, at
// https://www.embeddedarm.com/documentation/ts-enc720-782-schematic.pdf
//
// http://wiki.embeddedarm.com/wiki/TS-7200
#endif

// All the below registers for UART1
// First nine registers (up to Ox28) for UART 2

#define UART_DATA_OFFSET	0x0	// low 8 bits
#define DATA_MASK	0xff

// Status register. For more details see
// http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/s14/notes/l03.html
#define UART_RSR_OFFSET		0x4	// low 4 bits
// Frame Error: Stop bit did not come at the time expected. This means the
// clock is probably wrong.
#define FE_MASK		0x1 // frame error
// Parity Error: Parity bit does not match. Corrupt data.
#define PE_MASK		0x2 // parity error
// Break Error: Corresponds to the "break" key.
#define BE_MASK		0x4 // break error
// Overrun Error: Tried to write another byte into the TX register
// while the previous byte was still being sent out.
#define OE_MASK		0x8 // overrun error

#define UART_LCRH_OFFSET	0x8	// low 7 bits
#define BRK_MASK	0x1
#define PEN_MASK	0x2	// parity enable
#define EPS_MASK	0x4	// even parity
#define STP2_MASK	0x8	// 2 stop bits
#define FEN_MASK	0x10	// fifo
#define WLEN_MASK	0x60	// word length

#define UART_LCRM_OFFSET	0xc	// low 8 bits
#define BRDH_MASK	0xff	// MSB of baud rate divisor

#define UART_LCRL_OFFSET	0x10	// low 8 bits
#define BRDL_MASK	0xff	// LSB of baud rate divisor

#ifdef QEMU
#define UART_CTLR_OFFSET 0x38
#else
#define UART_CTLR_OFFSET	0x14	// low 8 bits
#endif
#define UARTEN_MASK	0x1
#define MSIEN_MASK	0x8	// modem status int
#define RIEN_MASK	0x10	// receive int
#define TIEN_MASK	0x20	// transmit int
#define RTIEN_MASK	0x40	// receive timeout int
#define LBEN_MASK	0x80	// loopback 

#define UART_FLAG_OFFSET	0x18	// low 8 bits
#define CTS_MASK	0x1
#define DCD_MASK	0x2
#define DSR_MASK	0x4
#define TXBUSY_MASK	0x8
#define RXFE_MASK	0x10	// Receive buffer empty
#define TXFF_MASK	0x20	// Transmit buffer full
#define RXFF_MASK	0x40	// Receive buffer full
#define TXFE_MASK	0x80	// Transmit buffer empty

#ifdef QEMU
#define UART_INTR_OFFSET 0x40
#define RIS_MASK    0x10
#define TIS_MASK    0x20
#define RTIS_MASK   0x40 // TODO: What is this even?
#define MIS_MASK    0x1
#else
#define UART_INTR_OFFSET	0x1c
#define RIS_MASK    0x2 // receive interrupt
#define TIS_MASK    0x4 // transmit interrupt
#define RTIS_MASK   0x8 // receive timeout interrupt
#define MIS_MASK    0x1 // modem interrupt
#endif

#define UART_DMAR_OFFSET	0x28

// Specific to UART1

#define UART_MDMCTL_OFFSET	0x100
#define UART_MDMSTS_OFFSET	0x104
#define UART_HDLCCTL_OFFSET	0x20c
#define UART_HDLCAMV_OFFSET	0x210
#define UART_HDLCAM_OFFSET	0x214
#define UART_HDLCRIB_OFFSET	0x218
#define UART_HDLCSTS_OFFSET	0x21c


