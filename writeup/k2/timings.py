#!/usr/bin/env python
import sys

names = [("Message Length", "4 bytes", "64 bytes"),
 ("Caches", "off", "on"),
 ("Send before Reply", "yes", "no"),
 ("Optimization", "off", "on")]
values = [85576, 183622, 57698, 116555, 82515, 180606, 55978, 114805,
		  32216, 69954, 22956, 45539, 32200, 67887, 21755, 44332]

USE_LATEX = len(sys.argv) > 1 and sys.argv[1] == 'latex'
if USE_LATEX:
    print '\\noindent\\begin{tabular}{|l|l|l|l||r|}'
    print '\\hline'
print ['\t', '&'][USE_LATEX].join([name[0] for name in names]
								  + ([] if USE_LATEX else ["Team"])
								  + ["Speed (us)"]),
if USE_LATEX: print '\\\\',
print

for i, val in enumerate(values):
	print ['\t', '&'][USE_LATEX].join(
		[names[j][1 + i / (2**j) % 2] for j, name in enumerate(names)]
			+ ([] if USE_LATEX else ["Peter and Justin"])
			+ [str(values[i]/200)]),
	if USE_LATEX: print '\\\\',
	print

if USE_LATEX:
    print '\\hline'
    print '\\end{tabular}'
