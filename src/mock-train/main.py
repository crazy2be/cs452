#!/usr/bin/python

import os
import random
import math

import pygame

def deg_to_rad(deg): return deg * math.pi / 180
def rot_center(image, rect, angle):
	"""rotate an image while keeping its center"""
	# http://www.pygame.org/wiki/RotateCenter
	rot_image = pygame.transform.rotate(image, angle)
	rot_rect = rot_image.get_rect(center=rect.center)
	return rot_image,rot_rect

class Train():
	def __init__(self):
		surf = pygame.Surface((100, 50))
		surf.fill((0, 0, 0))
		surf.set_colorkey((0, 0, 0))
		self.raw_image = surf
		self.raw_rect = pygame.Rect(0, 0, 100, 50)
		self.rot = 0
		self.off = (100, 100)
		pygame.draw.rect(surf, (100, 0, 0), self.raw_rect)

	def rotate(self, amount):
		self.rot += amount
		self.image, self.rect = rot_center(self.raw_image, self.raw_rect, self.rot)

	def move(self, amount):
		self.off = (self.off[0] + amount[0], self.off[1] + amount[1])

	def update(self):
		self.rotate(1)
		self.move((random.randint(-5, 5), random.randint(-5, 5)))

	def draw(self, surf):
		surf.blit(self.image, self.rect.move(self.off))

class Game(object):
	def __init__(self, surface):
		self.surface = surface
		self.clock = pygame.time.Clock()

	def start_screen(self):
		font = pygame.font.Font(None, 36)
		text = font.render("Hello There", 1, (10, 10, 10))

		rot = 45
		train = Train()
		while True:
			for event in pygame.event.get():
				if event.type == pygame.QUIT:
					return
			self.surface.fill((255, 255, 255))
			train.update()
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

	game = Game(surface)

	game.play_game()

	pygame.quit()

if __name__ == "__main__":
	main()
