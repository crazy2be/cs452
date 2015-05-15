.global enter_kernel_addr

.text
b Reset_Handler
b .
enter_kernel_addr: b enter_kernel
