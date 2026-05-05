#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Max vertices limited to 15 as per project requirements
#define MAX_V 15
// Value to represent disconnected nodes
#define INF 999999

// Structure to hold the graph using an adjacency matrix
typedef struct {
    int v_count;
    int matrix[MAX_V][MAX_V];
} City;

// Function prototypes for graph operations
void init_city(City* c, int v);
void add_road(City* c, int src, int dest, int w);
void print_path(int parent[], int j);
void run_dijkstra(City* c, int start, int end);

#endif