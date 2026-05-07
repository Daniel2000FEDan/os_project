#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_V 15
#define INF 999999

typedef struct {
    int v_count;
    int matrix[MAX_V][MAX_V];
} City;

// Structure to store the computed shortest path for GUI animation
typedef struct {
    int nodes[MAX_V];  // Array of node IDs in order (start to end)
    int count;         // Total number of nodes in the path
    int total_weight;
    bool found;        // True if a valid path exists
} Path;

void init_city(City* c, int v);
void add_road(City* c, int src, int dest, int w);
void print_path(int parent[], int j);
void run_dijkstra(City* c, int start, int end);

// Returns the path data structure instead of just printing it
Path get_shortest_path(City* c, int start, int end);

#endif