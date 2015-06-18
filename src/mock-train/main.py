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

class Game(object):
	def __init__(self, surface):
		self.surface = surface
		self.clock = pygame.time.Clock()

	def start_screen(self):
		font = pygame.font.Font(None, 36)
		text = font.render("Hello There", 1, (10, 10, 10))

		rot = 45
		while True:
			for event in pygame.event.get():
				if event.type == pygame.QUIT:
					return
			self.surface.fill((255, 255, 255))
			surf = pygame.Surface((100, 50))
			surf.fill((0, 0, 0))
			surf.set_colorkey((0, 0, 0))
			rect = pygame.Rect(0, 0, 100, 50)
			pygame.draw.rect(surf, (100, 0, 0), rect)
			surf2, rect2 = rot_center(surf, rect, rot)
			rot -= 1
			self.surface.blit(surf2, rect2.move(100, 100))
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
