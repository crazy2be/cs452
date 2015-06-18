CS452 Kernel
============
Coming to a trains lab near you soon!

<!---BEGIN LaTeX INCLUDED-->
Installation
------------
Simply run `make ENV=ts7200 install`. This will place the compiled kernel in
`/u/cs452/tftp/ARM/$(USER)/k.elf` (where `$(USER)` is whatever your username is
on the computer which you are compiling it). You can then load this kernel from
redboot using `load -b 0x00218000 -h 10.15.167.5 "ARM/$(USER)/k.elf"`.

Emulator
--------
You can run the OS on an emulated arm CPU via `make ENV=qemu qemu-run`. In
order for this to complete successfully, you will need:

 - A local install of QEMU
 - A local install of an arm-none-eabi toolchain

Tests
-----
The provided kernel has some basic tests, which you can run with
`make ENV=qemu test`. (this depends on the emulator, above).

Debugging
---------
If something goes wrong and you want to investigate deeper, you can run
`make ENV=qemu qemu-start` in one terminal (this starts qemu in a suspended
state), and then `make ENV=qemu qemu-debug` in another. You can then set
breakpoints, step, inspect memory, etc- anything you can normally do with gdb.

Special Instructions
--------------------

Before running the program on the TS7200, it is necessary to reset both the TS7200 and the
train controller, to avoid messy state left behind by previous runs.
In particular, this has been known to cause bugs with the sensor reads, where garbage data
left in the UART is picked up, and misinterpreted.

Useful links:
-------------

### General

 - [ARMv4 Manual](http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/pdf/arm-architecture.pdf)
 - [Terminal escape sequences](http://ascii-table.com/ansi-escape-sequences.php)

### TS7200 hardware

 - [TS7200 Manual](http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/pdf/ts-7200-manual.pdf)
 - [Cirrus EP9302 Manual](http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/pdf/ep93xx-user-guide.pdf)
 - [Interrupt controller manual](http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/pdf/icu-pl190.pdf)

### VersatilePB hardware emulated by QEMU

 - [SOC manual](http://infocenter.arm.com/help/topic/com.arm.doc.dui0224i/DUI0224I_realview_platform_baseboard_for_arm926ej_s_ug.pdf)
 - [Timer manual](http://infocenter.arm.com/help/topic/com.arm.doc.ddi0271d/DDI0271.pdf)
 - [Interrupt controller manual](http://infocenter.arm.com/help/topic/com.arm.doc.ddi0181e/DDI0181.pdf) (Same as the TS7200 hardware)
 - [UART Manual](http://infocenter.arm.com/help/topic/com.arm.doc.ddi0183f/DDI0183.pdf)
