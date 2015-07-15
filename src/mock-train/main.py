#!/usr/bin/python
import itertools
import numpy as np
import os
from numpy.random import randint
import numpy.random
from graph_tool import Graph
import graph_tool.draw
import track
from gi.repository import Gtk, Gdk, GdkPixbuf

import serial_interface
def main():
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
	v_weight = g.new_vertex_property("double")
	e_dist = g.new_edge_property("double")
	import pickle
	if os.path.isfile('previous_pos.pickle') and len(open('previous_pos.pickle').read()) > 0:
		pos = g.own_property(pickle.load(open('previous_pos.pickle')))
	else:
		print "Generating random layout."
		pos = g.new_vertex_property("vector<double>")
		for node in t:
			pos[g.vertex(node.i)] = (np.random.random(), np.random.random())

	middle_nodes = map("".join, itertools.product(["BR", "MR"], map(str, [153, 154, 155, 156]))) + ["EN1", "EN2", "EX1", "EX2"]
	for node in t:
		v = g.vertex(node.i)
		n_title[v] = node.name
		v_weight[v] = 1.0
		if node.name in middle_nodes: v_weight[v] = 0.0
		e = g.add_edge(g.vertex(node.i), g.vertex(node.reverse.i))
		e_weight[e] = 1.
		if node.typ == track.NODE_SENSOR: n_color[v] = "blue"
		elif node.typ == track.NODE_BRANCH: n_color[v] = "orange"
		elif node.typ == track.NODE_MERGE: n_color[v] = "yellow"
		elif node.typ == track.NODE_ENTER: n_color[v] = "green"
		elif node.typ == track.NODE_EXIT: n_color[v] = "red"
		else: n_color[v] = "white"
		for edge in node.edge:
			if edge.src is None: continue
			e = g.add_edge(g.vertex(edge.src.i), g.vertex(edge.dest.i))
			e_weight[e] = 1./(edge.dist**2)
			e_dist[e] = edge.dist
			e_title[e] = "%.2f" % (edge.dist)

	win = graph_tool.draw.GraphWindow(g, pos, (640, 480), edge_text=e_title, vertex_fill_color=n_color, vertex_text=n_title)
	win.show_all()
	def destroy_callback(*args, **kwargs):
		win.destroy()
		Gtk.main_quit()

	K = 1.
	layout_step = [K]
	# Use this draw instead if the shitty current layout doesn't satisfy and you
	# want to tweak it. With this enabled, the whole graph will morph you
	# drag a node, so that you don't have to drag each node individually.
	def relayout_draw(da, cr):
		graph_tool.draw.sfdp_layout(g, eweight=e_weight, pos=pos, max_iter=5,
									K=K, p=0.5, init_step=layout_step[0])
		layout_step[0] = layout_step[0] - math.log(layout_step[0])
		if da.vertex_matrix is not None:
			da.vertex_matrix.update()
		da.regenerate_surface(lazy=False)

	def set_switch(sw, d):
		for node in t:
			if node.typ == track.NODE_BRANCH and node.num == sw:
				node.switch_direction = d
				return
		print "WARN: Could not find switch %d" % sw

	class Train():
		num = -1
		speed = 0
		edge = t[0].edge[0]
		edge_dist = 0

		def __init__(self, num):
			self.num = num

		def update(self):
			e = g.edge(self.edge.src.i, self.edge.dest.i)
			self.edge_dist += self.speed
			if self.edge_dist > e_dist[e]:
				if self.edge.dest.typ == track.NODE_SENSOR:
					conn.set_sensor_tripped(self.edge.dest.num)
				self.edge = self.edge.dest.edge[self.edge.dest.switch_direction]
				self.edge_dist -= e_dist[e]

		def draw(self, pos, da, cr):
			e = g.edge(self.edge.src.i, self.edge.dest.i)
			start, end = np.array(pos[e.source()]), np.array(pos[e.target()])
			alpha = self.edge_dist / e_dist[e]
			pos = start + alpha*(end - start)
			dp = win.graph.pos_to_device(pos) # dp: device position
			cr.rectangle(dp[0], dp[1], 20, 20)
			cr.set_source_rgb(102. / 256, 102. / 256, 102. / 256)
			cr.fill()
			cr.move_to(dp[0], dp[1] + 20 - 12./2)
			cr.set_source_rgb(1., 1., 1.)
			cr.set_font_size(12)
			cr.show_text("%d" % self.num)
			cr.fill()

		def set_speed(self, speed): self.speed = speed
		def toggle_reverse(self):
			self.edge = self.edge.reverse
			self.edge_dist = e_dist[self.edge] - self.edge_dist

	def find_train(train_number):
		for train in trains:
			if train.num == train_number:
				return train
		train = Train(train_number)
		trains.append(train)
		return train

	trains = [Train(12)]
	def my_draw(da, cr):
		(typ, a1, a2) = conn.next_cmd()
		if typ is None: pass
		elif typ == 'set_speed': find_train(a1).set_speed(a2)
		elif typ == 'toggle_reverse': find_train(a1).toggle_reverse()
		elif typ == 'switch': set_switch(a1, a2)
		else: print "Ignoring command %s" % typ
		for train in trains:
			train.update()
			train.draw(pos, da, cr)
		da.queue_draw()

	win.connect("delete_event", destroy_callback)
	win.graph.connect("draw", my_draw)
	Gtk.main()
	pickle.dump(pos, open('previous_pos.pickle', 'w'))

main()
