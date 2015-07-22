NODE_NONE = 0
NODE_SENSOR = 1
NODE_BRANCH = 2
NODE_MERGE = 3
NODE_ENTER = 4
NODE_EXIT = 5

DIR_AHEAD = 0
DIR_STRAIGHT = 0
DIR_CURVED = 1

class TrackEdge():
  reverse = None # TrackEdge
  src = None # TrackNode
  dest = None #TrackNode
  dist = 0 # millimeters

class TrackNode():
  name = "no name"
  typ = -1
  num = -1
  reverse = None # TrackNode in the same location, but opposite direction.
  switch_direction = 0 # Current switch direction
  coord_x = 0.
  coord_y = 0.
  def __init__(self):
    self.edge = [TrackEdge(), TrackEdge()]

