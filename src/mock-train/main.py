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
	def pc(ttd, ps): return ps[ttd.piece]
	def start(ttd, pc): return pc.seg(ttd.segment)
	def end(ttd, pc): return pc.seg(ttd.segment+1)
	def vec(ttd, pc): return ttd.end(pc) - ttd.start(pc)
	def loc(ttd, pc):
		return ttd.start(pc).lerp(ttd.end(pc), ttd.offset / ttd.vec(pc).length)

GRAY = (100,100,100)
LIGHT_GRAY = (200,200,200)
RED = (255,0,0)
GREEN = (0,255,0)
BLUE = (0,0,255)

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
	track_types = [
		[v2(0,0), v2(0,1), v2(1,1), v2(1,0)],
		[v2(0,1), v2(0,0), v2(1,0), v2(1,1)]
	]
	def __init__(self, typ, off):
		self.ctrl = [p*100 + off for p in self.track_types[typ]]
		self.points = calculate_bezier(self.ctrl)

	def seg(self, i): return self.points[i]
	def nseg(self): return len(self.points)

	def draw(self, surf):
		### Draw control points
		for p in self.ctrl:
			pygame.draw.circle(surf, BLUE, p, 4)
		### Draw control "lines"
		pygame.draw.lines(surf, LIGHT_GRAY, False, self.ctrl)
		### Draw bezier segments
		pygame.draw.lines(surf, RED, False, self.points)

class Track():
	def __init__(self):
		self.pieces = [TrackPiece(0, (100, 100)), TrackPiece(1, (200, 0))]

	def advance(self, ttd, amount):
		ttd.offset += 10.0
		while True:
			pc = self.pieces[ttd.piece]
			if ttd.segment + 1 >= pc.nseg():
				ttd.segment = 0
				ttd.piece = (ttd.piece + 1) % len(self.pieces)
				continue
			d = ttd.vec(pc).length
			if ttd.offset < d: break
			ttd.offset -= d
			ttd.segment += 1

	def position(self, ttd): return ttd.loc(ttd.pc(self.pieces))
	def direction(self, ttd): return ttd.vec(ttd.pc(self.pieces))

	def draw(self, surf):
		for pc in self.pieces:
			pc.draw(surf)

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
		track = Track()
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
