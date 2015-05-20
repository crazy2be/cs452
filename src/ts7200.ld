ENTRY (main)
SECTIONS
{
 . = 0x00218000;
 .exception_vector . : { build/exception_vector.o }
 .startup . : { build/qemu.o } /* TODO: This shouldn't be called qemu.s/o */
 .text : { *(.text) }
 .data : { *(.data) }
 .bss : { __bss_start__ = . ; *(.bss) __bss_end__ = . ; }
 . = ALIGN(8);
 . = . + 0x1000; /* 4kB of stack memory */
 stack_top = . ;
}
