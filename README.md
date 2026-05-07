# OS Project - Traffic Simulation on a Graph

## Requirements
* GCC Compiler
* Make
* Raylib library

## How to Build
To build Milestone 1 (Algorithm logic):
`make milestone1`

To build Milestone 2 (Visual Simulation):
`make milestone2`

To clean build files:
`make clean`

## How to Run
Milestone 1:
`./dijkstra input.txt`

Milestone 2:
`./sim input.txt`

## Project Architecture
* `graph.c` / `graph.h`: Core data structures and Dijkstra implementation.
* `dijkstra.c`: CLI executable for algorithm validation.
* `sim.c`: Raylib graphical interface executable.
