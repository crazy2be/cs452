"""
bezier.py - Calculates a bezier curve from control points.

Depends on the 2d vector class found here: http://www.pygame.org/wiki/2DVectorClass

2007 Victor Blomqvist
Released to the Public Domain
"""
import pygame
import vec2d
def v2(x, y): return vec2d.Vec2d(x, y)
from pygame.locals import *

def calculate_bezier(p, steps = 10):
	"""
	Calculate a bezier curve from 4 control points and return a
	list of the resulting points.

	The function uses the forward differencing algorithm described here:
	http://www.niksula.cs.hut.fi/~hkankaan/Homepages/bezierfast.html
	"""
	t = 1.0 / steps
	temp = t*t

	f = v2(p[0][0], p[0][1])
	fd = 3 * (p[1] - p[0]) * t
	fdd_per_2 = 3 * (p[0] - 2 * p[1] + p[2]) * temp
	fddd_per_2 = 3 * (3 * (p[1] - p[2]) + p[3] - p[0]) * temp * t

	fddd = 2 * fddd_per_2
	fdd = 2 * fdd_per_2
	fddd_per_6 = fddd_per_2 / 3.0

	points = []
	for x in range(steps):
		points.append(v2(f[0], f[1]))
		f += fd + fdd_per_2 + fddd_per_6
		fd += fdd + fddd_per_2
		fdd += fddd
		fdd_per_2 += fddd_per_2
	points.append(f)
	return points

def main():
	gray = (100,100,100)
	lightgray = (200,200,200)
	red = (255,0,0)
	green = (0,255,0)
	blue = (0,0,255)
	X,Y,Z = 0,1,2
	pygame.init()
	screen = pygame.display.set_mode((600, 600))

	### Control points that are later used to calculate the curve
	control_points = [v2(100,100), v2(100,200), v2(200, 200), v2(200, 100), v2(300, 200), v2(300, 300), v2(300, 400), v2(400, 400), v2(500, 500), v2(400, 500)]

	### The currently selected point
	selected = None

	clock = pygame.time.Clock()

	running = True
	while running:
		for event in pygame.event.get():
			if event.type in (QUIT, KEYDOWN):
				running = False
			elif event.type == MOUSEBUTTONDOWN and event.button == 1:
				for p in control_points:
					if abs(p.x - event.pos[X]) < 10 and abs(p.y - event.pos[Y]) < 10:
						selected = p
			elif event.type == MOUSEBUTTONDOWN and event.button == 3:
				x,y = pygame.mouse.get_pos()
				control_points.append(v2(x,y))
			elif event.type == MOUSEBUTTONUP and event.button == 1:
				selected = None

		### Draw stuff
		screen.fill(gray)

		if selected is not None:
			selected.x, selected.y = pygame.mouse.get_pos()
			pygame.draw.circle(screen, green, selected, 10)

		### Draw control points
		for p in control_points:
			pygame.draw.circle(screen, blue, p, 4)
		### Draw control "lines"
		pygame.draw.lines(screen, lightgray, False, control_points)
		### Draw bezier curve
		for x in range(0,len(control_points)-1,3):
			b_points = calculate_bezier(control_points[x:x+4])
			#print b_points
			pygame.draw.lines(screen, red, False, b_points)

		### Flip screen
		pygame.display.flip()
		clock.tick(1000)
		#print clock.get_fps()

if __name__ == '__main__':
    main()
