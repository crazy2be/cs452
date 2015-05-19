ENTRY (main)
SECTIONS
{
 . = 0x00218000;
 .exception_vector . : { build/exception_vector.o }
 .startup . : { build/qemu.o }
 .text : { __text_start__ = . ; *(.text) __text_end__ = . ; }
 .data : { __data_start__ = . ; *(.data) __data_end__ = . ; }
 .bss : { __bss_start__ = . ; *(.bss) __bss_end__ = . ; }
 . = ALIGN(8);
 . = . + 0x1000; /* 4kB of stack memory */
 stack_top = . ;
}
