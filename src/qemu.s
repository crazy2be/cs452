@ This is the entry point for when we run on QEMU.
.global _Reset
_Reset:
 ldr sp, =stack_top
 bl main
 mov pc, lr @ TODO: This doesn't seem to work. Ideally it would make us exit qemu...
