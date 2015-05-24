CS452 Kernel
============
Coming to a trains lab near you soon!

<!---BEGIN LaTeX INCLUDED-->
Installation
------------
Simply run `make ENV=ts7200 install`. This will place the compiled kernel in
`/u/cs452/tftp/ARM/$(USER)/k.elf` (where `$(USER)` is whatever your username is
on the computer which you are compiling it). You can then load this kernel from
redboot using `load -b 0x00218000 -h 10.15.167.5 "ARM/$(USER)/k.elf"`. At this
stage, the kernel doesn't do much that is particularilly interesting, there is
no shell or other interactive elements, it simply runs the code which it was given.

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

