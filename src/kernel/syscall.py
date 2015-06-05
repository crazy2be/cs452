import sys
from os import path
syscalls = ["create", "pass", "exitk", "tid", "parent_tid",
			"send", "receive", "reply", "await", "rand", "should_idle"]

gen_dir = sys.argv[1]

asm = open(path.join(gen_dir, "syscalls.s"), 'w')
header = open(path.join(gen_dir, "syscalls.h"), 'w')
for i, syscall in enumerate(syscalls):
	asm.write(".equ SYSCALL_{0}, {1}\n".format(syscall.upper(), i))
	asm.write(".globl {0}\n".format(syscall))
	asm.write("{0}:\n".format(syscall))
	asm.write("	swi SYSCALL_{0}\n".format(syscall.upper()))
	asm.write("	bx lr\n\n")
	header.write("#define SYSCALL_{0} {1}\n".format(syscall.upper(), i))
