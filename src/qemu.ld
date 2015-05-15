/* Note that most of this file is inspired by
https://balau82.wordpress.com/2010/02/28/hello-world-for-bare-metal-arm-using-qemu/ */
ENTRY(_Reset)
SECTIONS
{
 . = 0x10000;
 .startup . : { build/qemu.o }
 .text : { *(.text) }
 .data : { *(.data) }
 .bss : { __bss_start__ = . ; *(.bss COMMON) __bss_end__ = .; }
 . = ALIGN(8);
 . = . + 0x1000; /* 4kB of stack memory */
 stack_top = .;
}
