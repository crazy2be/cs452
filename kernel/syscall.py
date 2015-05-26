syscalls = ["pass", "exitk", "create", "tid", "parent_tid",
			"send", "receive", "reply", "rand"]

asm = open("gen/syscall.s", 'w')
header = open("gen/syscalls.h", 'w')
for i, syscall in enumerate(syscalls):
	asm.write(".equ SYSCALL_{0}, {1}\n".format(syscall.upper(), i))
	asm.write(".globl {0}\n".format(syscall))
	asm.write("{0}:\n".format(syscall))
	asm.write("	swi SYSCALL_{0}\n".format(syscall.upper()))
	asm.write("	bx lr\n\n")
	header.write("#define SYSCALL_{0} {1}\n".format(syscall.upper(), i))
