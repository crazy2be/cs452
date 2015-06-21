#!/usr/bin/python

import os
import random
import math
import telnetlib
import socket
import errno

import pygame
import vec2d
def v2(x, y): return vec2d.Vec2d(x, y)
from bezier import calculate_bezier

def deg_to_rad(deg): return deg * math.pi / 180
def rot_center(image, rect, angle):
	"""rotate an image while keeping its center"""
	# http://www.pygame.org/wiki/RotateCenter
	rot_image = pygame.transform.rotate(image, angle)
	rot_rect = rot_image.get_rect(center=rect.center)
	return rot_image,rot_rect

class TrainSprite():
	def __init__(self):
		surf = pygame.Surface((10, 5))
		surf.fill((0, 0, 0))
		surf.set_colorkey((0, 0, 0))
		self.raw_image = surf
		self.raw_rect = pygame.Rect(0, 0, 10, 5)
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
	gray = (100,100,100)
	lightgray = (200,200,200)
	red = (255,0,0)
	green = (0,255,0)
	blue = (0,0,255)
	control_points = [v2(100,100), v2(100,200), v2(200, 200), v2(200, 100), v2(300, 200), v2(300, 300), v2(300, 400), v2(400, 400), v2(500, 500), v2(400, 500)]
	def __init__(self, train):
		self.train = train
		self.t = 0
		self.index = 0
		self.offset = 0
		self.points = []
		for x in range(0,len(self.control_points)-1,3):
			b_points = calculate_bezier(self.control_points[x:x+4])
			self.points += b_points

	def update(self):
		self.t += 1.0
		self.offset += 1.0
		v = v2(0, 0)
		d = 0
		while True:
			v = self.points[self.index + 1] - self.points[self.index]
			d = math.sqrt(v[0]*v[0] + v[1]*v[1])
			if self.offset < d: break
			self.offset -= d
			self.index += 1
		p = self.points[self.index] + v*(self.offset/d)
		x, y = p[0], p[1]
		print x, y
		self.train.rotate_to(-math.atan2(v[1], v[0]))
		self.train.move_to((x, y))

		# TODO: This is wrong
		#x = math.sin(self.t)
		#y = math.cos(self.t)

	def draw(self, surf):
		### Draw control points
		for p in self.control_points:
			pygame.draw.circle(surf, self.blue, p, 4)
		### Draw control "lines"
		pygame.draw.lines(surf, self.lightgray, False, self.control_points)
		### Draw bezier curve
		pygame.draw.lines(surf, self.red, False, self.points)

class Game(object):
	def __init__(self, surface, tn):
		self.surface = surface
		self.tn = tn
		self.clock = pygame.time.Clock()

	def start_screen(self):
		font = pygame.font.Font(None, 36)
		text = font.render("Hello There", 1, (10, 10, 10))

		rot = 45
		train = TrainSprite()
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
			track.draw(self.surface)
			train.draw(self.surface)
			textpos = text.get_rect()
			textpos.centerx = self.surface.get_rect().centerx
			self.surface.blit(text, textpos)
			pygame.display.flip()
			self.clock.tick(30)

	def play_game(self):
		self.start_screen()

def main():
	pygame.init()

	surface = pygame.display.set_mode([800, 600])
	pygame.mouse.set_visible(False)
	pygame.display.set_caption("Trains")

	tn = FakeTn()
	try:
		tn = telnetlib.Telnet("localhost", 1230)
	except socket.error, v:
		if v[0] == errno.ECONNREFUSED:
			# TODO: We probably don't want to catch this normally, just when
			# working on the train simulator.
			print "Warning: Telnet connection refused!"

	game = Game(surface, tn)

	game.play_game()

	pygame.quit()

class FakeTn():
	def read_eager(self):
		return ""

main()
