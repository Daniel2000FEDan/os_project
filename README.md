# OS Project - Traffic Simulation on a Graph

## Requirements
* GCC Compiler
* Make
* Raylib library

## Commands and Implementation by Milestones

### Milestone 1: Algorithm Logic
* **Build:** `make milestone1`
* **Run:** `./dijkstra <file_name>`
* **Implementation Description:** Implementation of the core graph data structure and Dijkstra's algorithm. The program reads a graph from a text file, builds the adjacency matrix/list, and accurately calculates the shortest path between the source and destination nodes, outputting the result to the console.

### Milestone 2: Visual Simulation (Static)
* **Build:** `make milestone2`
* **Run:** `./sim <file_name>`
* **Implementation Description:** Introduction of the Raylib graphical interface. The application reads the input file and renders a 2D static representation of the graph on the screen, displaying nodes, connecting edges, and edge weights visually.

### Milestone 3: Discrete Animation Engine
* **Build:** `make milestone3`
* **Run:** `./sim <file_name>`
* **Implementation Description:** Added a moving entity that travels along the shortest path calculated by Dijkstra. The movement includes precise timings: a full 1-second wait at each intermediate node, and movement along edges divided into discrete steps based on the edge's weight (each step taking exactly 300ms). Included Play/Stop toggles and a final "Destination Reached" display.

## Cleanup
* **Clean build files:** `make clean`
