#!/usr/bin/env python

import os, sys

if len(sys.argv) < 2:
	print("Please specify a command!")
	os.exit(1)

cmd = sys.argv[1]
if cmd == 'q':
	os.system('make ENV=qemu')
elif cmd == 'qr':
	os.system('make ENV=qemu qemu-run')
elif cmd == 'qs':
	os.system('make ENV=qemu qemu-start')
elif cmd == 'qd':
	os.system('make ENV=qemu qemu-debug')
elif cmd == 't':
	os.system('make ENV=qemu qemu-fast-test')
else:
	print "Unknown command"
