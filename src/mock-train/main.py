#!/usr/bin/python

import os
import random
import math
import telnetlib

import pygame

def deg_to_rad(deg): return deg * math.pi / 180
def rot_center(image, rect, angle):
	"""rotate an image while keeping its center"""
	# http://www.pygame.org/wiki/RotateCenter
	rot_image = pygame.transform.rotate(image, angle)
	rot_rect = rot_image.get_rect(center=rect.center)
	return rot_image,rot_rect

class TrainSprite():
	def __init__(self):
		surf = pygame.Surface((100, 50))
		surf.fill((0, 0, 0))
		surf.set_colorkey((0, 0, 0))
		self.raw_image = surf
		self.raw_rect = pygame.Rect(0, 0, 100, 50)
		self.rot = 0
		self.off = (100, 100)
		pygame.draw.rect(surf, (100, 0, 0), self.raw_rect)

	def rotate(self, drad): self.rotate_to(self.rad + drad)
	def rotate_to(self, rad):
		self.rad = rad
		deg = rad * 180 / math.pi
		self.image, self.rect = rot_center(self.raw_image, self.raw_rect, deg)

	def move(self, amount):
		self.off = (self.off[0] + amount[0], self.off[1] + amount[1])
	def move_to(self, loc): self.off = loc

	def update(self):
		self.rotate(1)
		self.move((random.randint(-5, 5), random.randint(-5, 5)))

	def draw(self, surf):
		surf.blit(self.image, self.rect.move(self.off))

class Train():
	pass

class TrackPiece():
	pass

class Track():
	def __init__(self, train):
		self.train = train
		self.t = 0

	def update(self):
		self.t += 0.1
		x = math.sin(self.t)
		y = math.cos(self.t)
		self.train.rotate_to(90-math.atan2(y, x))
		self.train.move_to((x*100 + 200, y*100 + 200))

class Game(object):
	def __init__(self, surface, tn):
		self.surface = surface
		self.tn = tn
		self.clock = pygame.time.Clock()

	def start_screen(self):
		font = pygame.font.Font(None, 36)
		text = font.render("Hello There", 1, (10, 10, 10))

		rot = 45
		train = Train()
		track = Track(train)
		cmd = ""
		while True:
			for event in pygame.event.get():
				if event.type == pygame.QUIT:
					return
			cmd += self.tn.read_eager()
			if len(cmd) > 1: print cmd
			if '\n' in cmd:
				s, cmd = cmd.split('\n', 1)
				text = font.render(s, 1, (10, 10, 10))
			self.surface.fill((255, 255, 255))
			track.update()
			#train.update()
			train.draw(self.surface)
			textpos = text.get_rect()
			textpos.centerx = self.surface.get_rect().centerx
			self.surface.blit(text, textpos)
			pygame.display.flip()
			self.clock.tick(30)

	def play_game(self):
		self.start_screen()

from bspline import Bspline
import pygame
from pygame import display, draw, PixelArray, Rect, Surface
from pygame.locals import *
import numpy as np

SCREEN_SIZE = (800, 600)
SCREEN = display.set_mode(SCREEN_SIZE)
STEP_N = 100

def main2():
	pygame.init()
	P = [(0, 100), (100, 0), (200, 0), (300, 100), (400, 200), (500, 200),
			(600, 100), (400, 400), (700, 50), (800, 200)]
	n = len(P) - 1 # n = len(P) - 1; (P[0], ... P[n])
	k = 3          # degree of curve
	m = n + k + 1  # property of b-splines: m = n + k + 1
	_t = 1. / (m - k * 2) # t between clamped ends will be evenly spaced
	# clamp ends and get the t between them
	t = k * [0] + [t_ * _t for t_ in xrange(m - (k * 2) + 1)] + [1] * k

	S = Bspline(P, t, k)
	# insert a knot (just to demonstrate the algorithm is working)
	#S.insert(0.9)

	surface = Surface(SCREEN_SIZE)
	pxarray = PixelArray(surface)
	for t_ in np.arange(0, 1, 1. / STEP_N):
		x, y = S(t_)
		pxarray[int(x), int(y)] = (255, 0, 0)
	del pxarray

	for p in zip(S.X, S.Y): draw.circle(surface, (0, 255, 0), [int(a) for a in p], 3, 0)
	SCREEN.blit(surface, (0, 0))

	while 1:
		for event in pygame.event.get():
			if event.type == pygame.QUIT:
				return
		display.update()
"""
bezier.py - Calculates a bezier curve from control points.

Depends on the 2d vector class found here: http://www.pygame.org/wiki/2DVectorClass

2007 Victor Blomqvist
Released to the Public Domain
"""
import pygame
from pygame.locals import *

from vec2d import *

def calculate_bezier(p, steps = 10):
	"""
	Calculate a bezier curve from 4 control points and return a
	list of the resulting points.

	The function uses the forward differencing algorithm described here:
	http://www.niksula.cs.hut.fi/~hkankaan/Homepages/bezierfast.html
	"""
	print len(p)
	t = 1.0 / steps
	temp = t*t

	f = p[0]
	fd = 3 * (p[1] - p[0]) * t
	fdd_per_2 = 3 * (p[0] - 2 * p[1] + p[2]) * temp
	fddd_per_2 = 3 * (3 * (p[1] - p[2]) + p[3] - p[0]) * temp * t

	fddd = 2 * fddd_per_2
	fdd = 2 * fdd_per_2
	fddd_per_6 = fddd_per_2 / 3.0

	points = []
	for x in range(steps):
		points.append(Vec2d(f[0], f[1]))
		f += fd + fdd_per_2 + fddd_per_6
		fd += fdd + fddd_per_2
		fdd += fddd
		fdd_per_2 += fddd_per_2
	points.append(f)
	return points

def vec2d(x, y): return Vec2d(x, y)
def main3():
	gray = (100,100,100)
	lightgray = (200,200,200)
	red = (255,0,0)
	green = (0,255,0)
	blue = (0,0,255)
	X,Y,Z = 0,1,2
	pygame.init()
	screen = pygame.display.set_mode((600, 600))

	### Control points that are later used to calculate the curve
	control_points = [vec2d(100,100), vec2d(100,200), vec2d(200, 200), vec2d(200, 100), vec2d(300, 200), vec2d(300, 300), vec2d(300, 400), vec2d(400, 400), vec2d(500, 500), vec2d(400, 500)]

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
				control_points.append(vec2d(x,y))
			elif event.type == MOUSEBUTTONUP and event.button == 1:
				selected = None

		### Draw stuff
		screen.fill(gray)

		if selected is not None:
			selected.x, selected.y = pygame.mouse.get_pos()
			pygame.draw.circle(screen, green, selected, 10)

		### Draw control points
		for p in control_points:
			pygame.draw.circle(screen, blue, (int(p[0]), int(p[1])), 4)
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
    main3()
    os.exit(0)

def main():
	pygame.init()

	surface = pygame.display.set_mode([800, 600])
	pygame.mouse.set_visible(False)
	pygame.display.set_caption("Trains")

	tn = telnetlib.Telnet("localhost", 1230)

	game = Game(surface, tn)

	game.play_game()

	pygame.quit()

if __name__ == "__main__":
	main2()
