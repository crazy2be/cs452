#!/usr/bin/python
import itertools
import numpy as np
import os
import time
from numpy.random import randint
import numpy.random
from graph_tool import Graph
import graph_tool.draw
import track
from gi.repository import Gtk, Gdk, GdkPixbuf

import serial_interface
def main():
	conn = serial_interface.connect()

	cur_track = track.init_tracka()
	g = Graph()
	g.add_vertex(len(cur_track))
	for (vi, node) in enumerate(cur_track): node.i = vi

	n_title = g.new_vertex_property("string")
	n_color = g.new_vertex_property("string")
	n_pos = g.new_vertex_property("vector<double>")
	e_title = g.new_edge_property("string")
	e_dist = g.new_edge_property("double")

	for node in cur_track:
		v = g.vertex(node.i)
		n_title[v] = node.name
		if node.typ == track.NODE_EXIT:
			# Invert points to match our ASCII display.
			n_pos[v] = (-node.reverse.coord_x, -node.reverse.coord_y)
		else:
			n_pos[v] = (-node.coord_x, -node.coord_y)
		e = g.add_edge(g.vertex(node.i), g.vertex(node.reverse.i))
		if node.typ == track.NODE_SENSOR: n_color[v] = "blue"
		elif node.typ == track.NODE_BRANCH: n_color[v] = "orange"
		elif node.typ == track.NODE_MERGE: n_color[v] = "yellow"
		elif node.typ == track.NODE_ENTER: n_color[v] = "green"
		elif node.typ == track.NODE_EXIT: n_color[v] = "red"
		else: n_color[v] = "white"
		for edge in node.edge:
			if edge.src is None: continue
			e = g.add_edge(g.vertex(edge.src.i), g.vertex(edge.dest.i))
			e_dist[e] = edge.dist
			e_title[e] = "%.2f" % (edge.dist)

	win = graph_tool.draw.GraphWindow(g, n_pos, (640, 480), edge_text=e_title, vertex_fill_color=n_color, vertex_text=n_title)
	win.show_all()
	def destroy_callback(*args, **kwargs):
		win.destroy()
		Gtk.main_quit()

	def set_switch(sw, d):
		for node in cur_track:
			if node.typ == track.NODE_BRANCH and node.num == sw:
				node.switch_direction = d
				return
		print "WARN: Could not find switch %d" % sw

	class Train():
		num = -1
		vel = 0.
		speed = 0.
		edge = cur_track[0].edge[0]
		edge_dist = 0
		SPEEDX = 1.

		def __init__(self, num):
			self.num = num

		def update(self):
			# Super shitty deacceleration model
			self.vel = self.vel + (0.018/self.SPEEDX)*(self.speed - self.vel)
			self.edge_dist += self.vel
			while True:
				e = self.e()
				if self.edge_dist < e_dist[e]: break
				if self.edge.dest.typ == track.NODE_SENSOR:
					conn.set_sensor_tripped(self.edge.dest.num)
				self.edge = self.edge.dest.edge[self.edge.dest.switch_direction]
				self.edge_dist -= e_dist[e]

		def draw(self, n_pos, da, cr):
			e = self.e()
			start, end = np.array(n_pos[e.source()]), np.array(n_pos[e.target()])
			alpha = self.edge_dist / e_dist[e]
			pos = start + alpha*(end - start)
			dp = win.graph.pos_to_device(pos) # dp: device position
			cr.rectangle(dp[0]-10, dp[1]-10, 20, 20)
			cr.set_source_rgb(102. / 256, 102. / 256, 102. / 256)
			cr.fill()
			cr.move_to(dp[0]-10, dp[1] + 10 - 12./2)
			cr.set_source_rgb(1., 1., 1.)
			cr.set_font_size(12)
			cr.show_text("%d" % self.num)
			cr.fill()
		def e(self): return g.edge(self.edge.src.i, self.edge.dest.i)
		def set_speed(self, speed): self.speed = speed/self.SPEEDX
		def toggle_reverse(self):
			self.edge = self.edge.reverse
			self.edge_dist = e_dist[self.e()] - self.edge_dist

	def find_train(train_number):
		for train in trains:
			if train.num == train_number:
				return train
		train = Train(train_number)
		trains.append(train)
		return train

	trains = [Train(12)]
	startup_time = time.time()
	accumulated_error = [0.]
	last_time = [time.time()]
	last_sensor_poll = [0]
	def my_draw(da, cr):
		(typ, a1, a2) = conn.next_cmd()
		if typ is None: pass
		elif typ == 'set_speed': find_train(a1).set_speed(a2)
		elif typ == 'toggle_reverse': find_train(a1).toggle_reverse()
		elif typ == 'switch': set_switch(a1, a2)
		elif typ == 'sensor': last_sensor_poll[0] = round((time.time() - startup_time) * 1000)/1000
		else: print "Ignoring command %s" % typ
		cur_time = time.time()
		dt = cur_time - last_time[0] + accumulated_error[0]
		num_steps = int(dt/ 0.05)
		accumulated_error[0] = dt - num_steps*0.05
		print dt, cur_time, last_time[0], num_steps, accumulated_error[0]
		for train in trains:
			for _ in range(0, num_steps): train.update()
			train.draw(n_pos, da, cr)
			cr.move_to(10., 10.)
			cr.set_source_rgb(0., 0., 0.)
			cr.set_font_size(12)
			cr.show_text("Last polled at %.3f" % last_sensor_poll[0])
		da.queue_draw()
		last_time[0] = cur_time

	win.connect("delete_event", destroy_callback)
	win.graph.connect("draw", my_draw)
	Gtk.main()

main()

# Prototype a* junk. You can just ignore this...
def astar(start, end):
	import math
	def min_heap_push(mh, key, val):
		mh.append({'key': key, 'val': val})

	def min_heap_pop(mh):
		min_f = 1000000000.
		min_i = -1
		for (i, e) in enumerate(mh):
			if e['key'] < min_f:
				min_f = e['key']
				min_i = i
		min_val = mh[min_i]['val']
		del mh[min_i]
		return min_val

	def h(start, end):
		dx, dy = start.coord_x - end.coord_x, start.coord_y - end.coord_y
		return math.sqrt(dx*dx + dy*dy)

	def reconstruct_path(node_parents, current):
		path = []
		while current is not None:
			if current in path:
				raise Exception("cycle at %d" % current.i)
			path.insert(0, current)
			current = node_parents[current.i]
		return path

	mh = []
	min_heap_push(mh, 0., start.i)
	node_g = [100000000.]*len(cur_track)
	node_f = [100000000.]*len(cur_track)
	node_g[start.i], node_f[start.i] = 0., 0.
	node_parents = [None]*len(cur_track)

	while len(mh) > 0:
		min_i = min_heap_pop(mh)
		q = cur_track[min_i]
		for suc_edge in q.edge:
			suc = suc_edge.dest
			if suc is None:
				continue
			suc_g = node_g[q.i] + suc_edge.dist
			suc_f = suc_g + h(suc, end)
			if node_f[suc.i] < suc_f:
				continue
			node_g[suc.i], node_f[suc.i] = suc_g, suc_f
			node_parents[suc.i] = q
			if suc == end:
				return reconstruct_path(node_parents, suc)
			min_heap_push(mh, suc_f, suc.i)
	return None

#print cur_track[0].name, cur_track[1].name
#print map(lambda n: n and n.name, astar(cur_track[0], cur_track[1]))
