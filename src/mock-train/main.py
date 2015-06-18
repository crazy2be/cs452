#!/usr/bin/python

import os
import random
import math

import pygame

class Game(object):
	def __init__(self, surface):
		self.surface = surface
		self.clock = pygame.time.Clock()

	def start_screen(self):
		font = pygame.font.Font(None, 36)
		text = font.render("Hello There", 1, (10, 10, 10))

		while True:
			for event in pygame.event.get():
				if event.type == pygame.QUIT:
					return
			self.surface.fill((255, 255, 255))
			textpos = text.get_rect()
			textpos.centerx = self.surface.get_rect().centerx
			self.surface.blit(text, textpos)
			pygame.display.flip()

	def play_game(self):
		self.start_screen()

def main():
	pygame.init()

	surface = pygame.display.set_mode([800, 600])
	pygame.mouse.set_visible(False)
	pygame.display.set_caption("Argh, it's the Asteroids!!")

	game = Game(surface)

	game.play_game()

	pygame.quit()

if __name__ == "__main__":
	main()
