import sys
from os import path
syscalls = ["try_create", "pass", "exitk", "tid", "parent_tid",
			"try_send", "try_receive", "try_reply", "try_await", "rand", "should_idle", "halt"]

gen_dir = sys.argv[1]

asm = open(path.join(gen_dir, "syscalls.s"), 'w')
header = open(path.join(gen_dir, "syscalls.h"), 'w')
header.write("#pragma once\n")
for i, syscall in enumerate(syscalls):
	ns = (syscall[len("try_"):] if "try_" in syscall else syscall).upper()
	asm.write(".equ SYSCALL_{0}, {1}\n".format(ns, i))
	asm.write(".globl {0}\n".format(syscall))
	asm.write("{0}:\n".format(syscall))
	asm.write("	swi SYSCALL_{0}\n".format(ns))
	asm.write("	bx lr\n\n")
	header.write("#define SYSCALL_{0} {1}\n".format(ns, i))
