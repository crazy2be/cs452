ENTRY (main)
SECTIONS
{
 . = 0x00218000;
 .exception_vector . : { build/kernel/exception_vector.o }
 .startup . : { build/kernel/qemu.o } /* TODO: This shouldn't be called qemu.s/o */
 .text : { *(.text) }
 .data : { *(.data) }
 .uninitializedbss : { build/kernel/stack.o(.bss) }
 .bss : { __bss_start__ = . ; *(.bss) __bss_end__ = . ; }
 . = ALIGN(8);
 . = . + 0x1000; /* 4kB of stack memory */
 stack_top = . ;
}
