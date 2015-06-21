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

class TrainTrackData():
	def __init__(ttd):
		ttd.piece = 0 # Current track piece we are on
		ttd.segment = 0 # Current segment in the current piece
		ttd.offset = 0 # Current offset along the segment
	def dist(ttd, ps): return ttd.dir(ps).length
	def p(ttd, ps): return ps[ttd.piece][ttd.segment]
	def dir(ttd, ps): return ps[ttd.piece][ttd.segment+1] - ttd.p(ps)

class Train():
	def __init__(self):
		self.sprite = TrainSprite()
		self.ttd = TrainTrackData()

	def update(self, track):
		track.advance(self.ttd, 10.0)
		direction = track.direction(self.ttd)
		position = track.position(self.ttd)
		self.sprite.move_to(position)
		self.sprite.rotate_to(-math.atan2(direction[1], direction[0]))

	def draw(self, surf): self.sprite.draw(surf)

class TrackPiece():
	ctrl = [v2(100,100), v2(100,200), v2(200, 200), v2(200, 100)]
	def __init__(self):
		self.index = 0

class Track():
	gray = (100,100,100)
	lightgray = (200,200,200)
	red = (255,0,0)
	green = (0,255,0)
	blue = (0,0,255)
	track_pieces = [
		[v2(100,100), v2(100,200), v2(200, 200), v2(200, 100)],
		[v2(200, 100), v2(300, 200), v2(300, 300), v2(300, 400)],
		[v2(300, 400), v2(400, 400), v2(500, 500), v2(400, 500)]]
	def __init__(self, train):
		self.train = train
		self.points = []
		for x in range(0, len(self.track_pieces)):
			b_points = calculate_bezier(self.track_pieces[x])
			self.points.append(b_points)

	def advance(self, ttd, amount):
		ps = self.points
		ttd.offset += 10.0
		while True:
			if ttd.segment + 1 >= len(self.points[ttd.piece]):
				ttd.segment = 0
				ttd.piece = (ttd.piece + 1) % len(self.track_pieces)
			d = ttd.dist(ps)
			if ttd.offset < d: break
			ttd.offset -= d
			ttd.segment += 1

	def position(self, ttd):
		ps = self.points
		return ttd.p(ps) + ttd.dir(ps)*(ttd.offset/ttd.dist(ps))
	def direction(self, ttd): return ttd.dir(self.points)

	def draw(self, surf):
		for i, track_points in enumerate(self.track_pieces):
			### Draw control points
			for p in track_points:
				pygame.draw.circle(surf, self.blue, p, 4)
			### Draw control "lines"
			pygame.draw.lines(surf, self.lightgray, False, track_points)
			### Draw bezier segments
			pygame.draw.lines(surf, self.red, False, self.points[i])

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
			#track.update()
			train.update(track)
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
