#!/usr/bin/env python
import sys

names = [("Message Length", "4 bytes", "64 bytes"),
 ("Caches", "off", "on"),
 ("Send before Reply", "yes", "no"),
 ("Optimization", "off", "on")]
values = [85576, 183622, 57698, 116555, 82515, 180606, 55978, 114805,
		  32216, 69954, 22956, 45539, 32200, 67887, 21755, 44332]

for i, val in enumerate(values):
	mlen = names[0][1 + i % 2]
	caches = i / 2 % 2
	sendb4 = i / 4 % 2
	opt = i / 8 % 2
	print mlen, caches, sendb4, opt, val
