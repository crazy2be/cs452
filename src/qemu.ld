ENTRY(_Reset)
SECTIONS
{
 . = 0x0;
 .exception_vector . : { build/exception_vector.o }
 . = 0x10000;
 .startup . : { build/qemu.o }
 .text : { *(.text) }
 .data : { *(.data) }
 .bss : { __bss_start__ = . ; *(.bss COMMON) __bss_end__ = .; }
 . = ALIGN(8);
 . = . + 0x1000; /* 4kB of stack memory */
 stack_top = .;
}
