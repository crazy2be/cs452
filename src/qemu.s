@ This is the entry point for when we run on QEMU. Inspired by
@ https://balau82.wordpress.com/2010/02/28/hello-world-for-bare-metal-arm-using-qemu/
@ TODO: may want to augment this with learnings from
@ http://www.embedded.com/design/mcus-processors-and-socs/4026075/Building-Bare-Metal-ARM-Systems-with-GNU-Part-2
.global Reset_Handler
Reset_Handler:
 ldr sp, =stack_top
 bl main
 mov pc, lr @ TODO: This doesn't seem to work. Ideally it would make us exit qemu...
