CC = gcc
CFLAGS = -Wall -Wextra -std=c99
RAYLIB_FLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

all: milestone1 milestone2 milestone3

milestone1: dijkstra.c graph.c
	$(CC) $(CFLAGS) dijkstra.c graph.c -o dijkstra

milestone2: sim.c graph.c
	$(CC) $(CFLAGS) sim.c graph.c -o sim $(RAYLIB_FLAGS)

milestone3: sim.c graph.c
	$(CC) $(CFLAGS) sim.c graph.c -o sim $(RAYLIB_FLAGS)



clean:
	rm -f dijkstra sim