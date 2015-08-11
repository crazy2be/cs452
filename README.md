CS452 Kernel
============
Coming to a trains lab near you soon!

<!---BEGIN LaTeX INCLUDED-->
Installation
------------
On the student cs environment, simply run `make -j8 ENV=ts7200 TYPE=t install`.
This will place the compiled kernel in `/u/cs452/tftp/ARM/$(USER)/t.elf`
(where `$(USER)` is whatever your username is on the computer which you are
compiling it). You can then load this kernel from
redboot using `load -b 0x00218000 -h 10.15.167.5 "ARM/$(USER)/t.elf"`.

Note that this depends on a toolchain installed in my home directory,
`/u1/jmcgirr/arm-toolchain`. This is currently world-readable, so it should
work regardless of who is trying to run the Makefile, but please do let us
know if you encounter any issues!

Emulator
--------
You can run the OS on an emulated arm CPU via `make -j8 ENV=qemu TYPE=t qemu-run`. In
order for this to complete successfully, you will need:

 - A local install of QEMU
 - A local install of an arm-none-eabi toolchain

Tests
-----
The provided kernel has some basic tests, which you can run with
`make qemu-test`. (this depends on the emulator, above).

Debugging
---------

If something goes wrong and you want to investigate deeper, you can run
`make qemu-start` in one terminal (this starts qemu in a suspended
state), and then `make qemu-debug` in another. You can then set
breakpoints, step, inspect memory, etc- anything you can normally do with gdb.

A simulator for the track itself is also available in `src/mock-train/main.py`.
It requires installing [graph-tool](https://graph-tool.skewed.de/).
To run it, run

    make qemu-run

    # in another window
    python src/mock-train/main.py

    # in a third window
    telnet localhost 1231

This display a graphical view of the simulated train set.
The simulator communicates with the emulated program with the same binary protocol
as the real train controller (although it processes input faster), so that it's
transparent to the program whether it's talking to the real train set, or the
simulated one.

The simulator is by no means perfect, and there are many failure cases which
aren't reproducible on the simulator.
The simulated trains have different acceleration, deceleration, and velocity
characteristics than the real trains.
In particular, the simulated trains aren't affected by track conditions, so
the velocity of the train doesn't vary across sections of track.
Also, trains are treated as being infinitesimally small points, instead of
having size.
This allows performing maneuvers in the simulated trains which would result
in a crash on the real track.

Despite its limitations, the simulator is an extremely valuable tool for rapidly
debugging routing or track reservation issues, because it's faster to iterate
on a simulator than to load and run the code on the real hardware.
The advantage is particularly striking when the track is heavily contended.

Commands
--------

 - `tr <train number> <speed>` sets the speed of a train.
 - `sw <switch number> (c|s)` sets a the position of a switch to curved or straight.
 - `bsw (c|s)` bulk set all switches to the given configuration (curved or straight).
 - `rv <train number>` reverses a train.
 - `stop <train number> <node name> [<edge>] <displacement>` stops the train at
   the given node, or offset thereof. I.e. `stp 63 E8 0` would stop train 63
   right on top of sensor E8. If `<edge>` is specified, it denotes which edge
   the offset is along (i.e. for a branch node).
 - `bisect <train number>` performs bisection to calibrate the stopping distance
   for `<train number>`, exiting automatically after 5 iterations.
 - `route <train number> <node name>` routes the given train to a particular node.
   Currently, the train must be fully stopped for the routing to work properly.
   It's also necessary to run the train manually until it hits a sensor, so
   that the program picks up the position of the train.
 - `f` Freezes the terminal output until typed again. This is useful for
   copying log data out of the terminal program without it being overwritten.
 - `q` exits the program.

The positions of each switch is shown in the ASCII-art map of the train track.
When a sensor is tripped, it is shown both on the ASCII-art map for as long as
it is tripped, as well as to the right, on the list of recently-fired sensors.

Because sensor attribution is fault-tolerant, the program will recover if a single
switch fails or a sensor fails to register.
After the train hits the next switch, the program will recognize the train's new
position, and correctly display it.
Furthermore, if a switch failed, the program will correctly recognize that the
switch is in the opposite orientation it thought it was in, and update the
internal model.
The program is also tolerant of spurious sensor hits, and will ignore sensor
hits that it thinks weren't caused by a train.

<!--END LaTeX INCLUDED-->

Useful links:
-------------

### General

 - [ARMv4 Manual](http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/pdf/arm-architecture.pdf)
 - [Terminal escape sequences](http://ascii-table.com/ansi-escape-sequences.php)

### TS7200 hardware

 - [TS7200 Manual](http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/pdf/ts-7200-manual.pdf)
 - [Cirrus EP9302 Manual](http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/pdf/ep93xx-user-guide.pdf)
 - [Interrupt controller manual](http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/pdf/icu-pl190.pdf)

### VersatilePB hardware emulated by QEMU

 - [Hello World Tutorial](https://balau82.wordpress.com/2010/02/28/hello-world-for-bare-metal-arm-using-qemu/)
 - [SOC manual](http://infocenter.arm.com/help/topic/com.arm.doc.dui0224i/DUI0224I_realview_platform_baseboard_for_arm926ej_s_ug.pdf)
 - [Timer manual](http://infocenter.arm.com/help/topic/com.arm.doc.ddi0271d/DDI0271.pdf)
 - [Interrupt controller manual](http://infocenter.arm.com/help/topic/com.arm.doc.ddi0181e/DDI0181.pdf) (Same as the TS7200 hardware)
 - [UART Manual](http://infocenter.arm.com/help/topic/com.arm.doc.ddi0183f/DDI0183.pdf)
