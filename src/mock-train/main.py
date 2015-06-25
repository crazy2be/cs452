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
	def __str__(ttd):
		return "piece: {0}, segment: {1}, offset: {2}".format(ttd.piece, ttd.segment, ttd.offset)
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
		self.s = 5.
		self.direction = -1.

	def update(self, track):
		track.advance(self.ttd, self.s*self.direction)
		direction = track.direction(self.ttd)
		position = track.position(self.ttd)
		self.sprite.move_to(position)
		self.sprite.rotate_to(-math.atan2(direction[1], direction[0]))

	def set_speed(self, speed):
		# TODO: Smooth function here
		self.s = speed

	def toggle_reverse(self): self.direction *= -1

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
		# Draw control points
		for p in self.ctrl:
			pygame.draw.circle(surf, BLUE, p, 4)
		# Draw control "lines"
		pygame.draw.lines(surf, LIGHT_GRAY, False, self.ctrl)
		# Draw bezier segments
		pygame.draw.lines(surf, RED, False, self.points)

class Track():
	def __init__(self):
		self.pieces = [TrackPiece(0, (100, 100)), TrackPiece(1, (200, 0))]

	def advance(self, ttd, amount):
		def mod(n, m): return ((n % m) + m) % m
		ttd.offset += amount
		# TODO: This code is pretty gross. It would be nice to clean it
		# up at least a little bit... I feel like we aught to be able to
		# better generalize forward/backward travel at the very least.
		if amount < 0:
			while True:
				pc = self.pieces[ttd.piece]
				if ttd.segment < 0:
					ttd.piece = mod(ttd.piece - 1, len(self.pieces))
					ttd.segment = self.pieces[ttd.piece].nseg() - 2
					pc = self.pieces[ttd.piece]
					ttd.offset += ttd.vec(pc).length
					continue
				if ttd.offset < 0:
					ttd.segment -= 1
					if ttd.segment >= 0:
						ttd.offset += ttd.vec(pc).length
					continue
				return
		else:
			while True:
				pc = self.pieces[ttd.piece]
				if ttd.segment + 1 >= pc.nseg():
					ttd.piece = mod(ttd.piece + 1, len(self.pieces))
					ttd.segment = 0
					continue
				d = ttd.vec(pc).length
				if ttd.offset >= d:
					ttd.offset -= d # length of current segment
					ttd.segment += 1
					continue
				return

	def position(self, ttd): return ttd.loc(ttd.pc(self.pieces))
	def direction(self, ttd): return ttd.vec(ttd.pc(self.pieces))

	def draw(self, surf):
		for pc in self.pieces:
			pc.draw(surf)

import socket, select
class MyTelnet():
	def __init__(self, host, port=23, timeout=socket._GLOBAL_DEFAULT_TIMEOUT):
		self.sock = socket.create_connection((host, port), timeout)
		self.wbuf = ''
		self.rbuf = ''
	def fileno(self): return self.sock.fileno()
	def read(self): buf = self.rbuf; self.rbuf = ''; return buf
	def write(self, buf): self.wbuf += buf
	def service(self):
		if len(self.wbuf) > 0 and select.select([], [self], [], 0) == ([], [self], []):
			sent = self.sock.send(self.wbuf)
			self.wbuf = self.wbuf[sent:]
		elif select.select([self], [], [], 0) == ([self], [], []):
			self.rbuf += self.sock.recv(50)
class MyTelnetWrapper():
	def __init__(self, host, port=23, timeout=socket._GLOBAL_DEFAULT_TIMEOUT):
		self.tn = telnetlib.Telnet(host, port, timeout)
	def read(self): return self.tn.read_eager()
	def write(self, buf): return self.tn.write(buf) # TODO: Non-blocking
	def service(self): pass
class FakeTelnet():
	def __init__(self): pass
	def read(self): return '\x00'
	def write(self, buf): pass
	def service(self): pass
class CmdParser():
	def __init__(self, tn, train):
		self.tn = tn
		self.train = train
		self.s = ''
		while True:
			self.s += self.tn.read()
			if len(self.s) > 0:
				assert(self.s[0] == '\x00')
				self.s = self.s[1:]
				break

	def parse_cmd(self):
		self.s += self.tn.read()
		self.s = self._parse_cmd(self.s)

	def _parse_cmd(self, s):
		if len(s) < 1: return s
		print "Got command %s"% s.encode('hex')
		f = ord(s[0])
		if 0 <= f <= 14:
			if len(s) < 2: return s
			print "Setting speed of %d to %d" % (ord(s[1]), f)
			self.train.set_speed(f)
			return s[2:]
		elif f == 15:
			if len(s) < 2: return s
			print "Toggling reverse of %d" % ord(s[1])
			self.train.toggle_reverse()
			self.tn.write('\x00'*5 + '\x10' + '\x00'*4)
			return s[2:]
		elif f == 0x20:
			print "Disabling solenoid"
			return s[1:]
		elif 0x21 <= f <= 0x22:
			if len(s) < 2: return s
			print "Switch command not supported %d %d" % (f, ord(s[1]))
			return s[2:]
		elif f == 0x85:
			print "Got sensor poll"
			return s[1:]
		raise Exception("Unknown command %s" % s.encode('hex'))


class Game(object):
	def __init__(self, surface, tn):
		self.surface = surface
		self.tn = tn
		self.clock = pygame.time.Clock()
		self.train = Train()

	def start_screen(self):
		font = pygame.font.Font(None, 36)
		text = font.render("Hello There", 1, (10, 10, 10))

		rot = 45
		track = Track()
		cmd = CmdParser(self.tn, self.train)
		while True:
			for event in pygame.event.get():
				if event.type == pygame.QUIT:
					return
			self.tn.service()
			cmd.parse_cmd()
			self.surface.fill((255, 255, 255))
			#track.update()
			self.train.update(track)
			track.draw(self.surface)
			self.train.draw(self.surface)
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

	tn = FakeTelnet()
	try:
		tn = MyTelnetWrapper("localhost", 1230)
	except socket.error, v:
		if v[0] == errno.ECONNREFUSED:
			# TODO: We probably don't want to catch this normally, just when
			# working on the train simulator.
			print "Warning: Telnet connection refused!"

	game = Game(surface, tn)

	game.play_game()

	pygame.quit()

main()
