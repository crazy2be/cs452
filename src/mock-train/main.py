#!/usr/bin/python

from __future__ import division
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

SCREEN_SIZE = (800, 600)
SCREEN = display.set_mode(SCREEN_SIZE)
STEP_N = 1000

def main2():
	pygame.init()
	rect = Rect((0, 0), SCREEN_SIZE)
	surface = Surface(SCREEN_SIZE)
	pxarray = PixelArray(surface)
	P = [(0, 100), (100, 0), (200, 0), (300, 100), (400, 200), (500, 200),
			(600, 100), (400, 400), (700, 50), (800, 200)]
	n = len(P) - 1 # n = len(P) - 1; (P[0], ... P[n])
	k = 3          # degree of curve
	m = n + k + 1  # property of b-splines: m = n + k + 1
	_t = 1 / (m - k * 2) # t between clamped ends will be evenly spaced
	# clamp ends and get the t between them
	t = k * [0] + [t_ * _t for t_ in xrange(m - (k * 2) + 1)] + [1] * k

	S = Bspline(P, t, k)
	# insert a knot (just to demonstrate the algorithm is working)
	S.insert(0.9)

	step_size = 1 / STEP_N
	for i in xrange(STEP_N):
		t_ = i * step_size
		try: x, y = S(t_)
		# if curve not defined here (t_ is out of domain): skip
		except AssertionError: continue
		x, y = int(x), int(y)
		pxarray[x][y] = (255, 0, 0)
	del pxarray

	for p in zip(S.X, S.Y): draw.circle(surface, (0, 255, 0), [int(a) for a in p], 3, 0)
	SCREEN.blit(surface, (0, 0))

	while 1:
		for event in pygame.event.get():
			if event.type == pygame.QUIT:
				return
		display.update()

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
