#!/usr/bin/python

import os
import random
import math

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

class Game(object):
	def __init__(self, surface, conn, g, pos, n_title):
		self.surface = surface
		self.conn = conn
		self.g = g
		self.pos = pos
		self.n_title = n_title
		self.clock = pygame.time.Clock()
		self.train = Train()

	def start_screen(self):
		font = pygame.font.Font(None, 36)
		text = font.render("Hello There", 1, (10, 10, 10))

		rot = 45
		track = Track()
		while True:
			for event in pygame.event.get():
				if event.type == pygame.QUIT:
					return
			(typ, a1, a2) = self.conn.next_cmd()
			if typ is None: pass
			elif typ == 'set_speed': self.train.set_speed(a2)
			elif typ == 'toggle_reverse': self.train.toggle_reverse()
			else: print "Ignoring command %s" % typ
			self.surface.fill((255, 255, 255))
			#track.update()
			self.train.update(track)
			for v in self.g.vertices():
				p = self.pos[v]
				p = (int(p[0]*50), int(p[1]*50))
				pygame.draw.circle(self.surface, (100, 100, 100), p, 10)
				self.surface.blit(font.render(self.n_title[v], 1, (10, 10, 10)), p)
			track.draw(self.surface)
			self.train.draw(self.surface)
			textpos = text.get_rect()
			textpos.centerx = self.surface.get_rect().centerx
			self.surface.blit(text, textpos)
			pygame.display.flip()
			self.clock.tick(30)

	def play_game(self):
		self.start_screen()

from itertools import izip
import numpy as np
from numpy.random import randint
import numpy.random
from graph_tool import Graph
import graph_tool.draw
import track
from gi.repository import Gtk, Gdk, GdkPixbuf

import serial_interface
def main():
	pygame.init()

	surface = pygame.display.set_mode([800, 600])
	pygame.mouse.set_visible(False)
	pygame.display.set_caption("Trains")

	conn = serial_interface.connect()

	t = track.init_tracka()
	g = Graph()
	g.add_vertex(len(t))
	for (vi, node) in enumerate(t):
		node.i = vi

	n_title = g.new_vertex_property("string")
	n_color = g.new_vertex_property("string")
	e_title = g.new_edge_property("string")
	e_weight = g.new_edge_property("double")
	import pickle
	if os.path.isfile('previous_pos.pickle') and len(open('previous_pos.pickle').read()) > 0:
		previous_pos = g.own_property(pickle.load(open('previous_pos.pickle')))
	else:
		print "Generating random layout."
		previous_pos = g.new_vertex_property("vector<double>")
		for node in t:
			previous_pos[g.vertex(node.i)] = (np.random.random(), np.random.random())

	for node in t:
		v = g.vertex(node.i)
		n_title[v] = node.name
		e = g.add_edge(g.vertex(node.i), g.vertex(node.reverse.i))
		e_weight[e] = 0.0
		if node.typ == track.NODE_SENSOR: n_color[v] = "blue"
		elif node.typ == track.NODE_BRANCH: n_color[v] = "orange"
		elif node.typ == track.NODE_MERGE: n_color[v] = "yellow"
		elif node.typ == track.NODE_ENTER: n_color[v] = "green"
		elif node.typ == track.NODE_EXIT: n_color[v] = "red"
		else: n_color[v] = "white"
		for edge in node.edge:
			if edge.src is None: continue
			e = g.add_edge(g.vertex(edge.src.i), g.vertex(edge.dest.i))
			e_weight[e] = edge.dist
			e_title[e] = "%.2f" % (edge.dist)

	#pos = graph_tool.draw.fruchterman_reingold_layout(g, weight=e_weight, pos=previous_pos)
	pos = graph_tool.draw.sfdp_layout(g, eweight=e_weight, pos=previous_pos)
	#new_pos, _ = graph_tool.draw.interactive_window(g, pos=pos, vertex_text=n_title,
	#								   edge_text=e_title, vertex_fill_color=n_color,
	#								   cr=surface)
	win = graph_tool.draw.GraphWindow(g, pos, (400, 400), update_layout=True, edge_text=e_title, vertex_fill_color=n_color, vertex_text=n_title)
	win.show_all()
	def destroy_callback(*args, **kwargs):
		win.destroy()
		Gtk.main_quit()
	def my_draw(da, cr):
		print "Draw called with ", da, cr
		cr.rectangle(7, 7, 7, 7)
		cr.set_source_rgb(102. / 256, 102. / 256, 102. / 256)
		cr.fill()
	win.connect("delete_event", destroy_callback)
	win.graph.connect("draw", my_draw)
	Gtk.main()
	#pickle.dump(new_pos, open('previous_pos.pickle', 'w'))
	game = Game(surface, conn, g, pos, n_title)
	game.play_game()

	pygame.quit()

main()
sys.exit(0)
