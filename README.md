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
