#include "graph.h"

// Initialize the graph, setting self-loops to 0 and other edges to INF
void init_city(City* c, int v) {
    c->v_count = v;
    for (int i = 0; i < v; i++) {
        for (int j = 0; j < v; j++) {
            c->matrix[i][j] = (i == j) ? 0 : INF;
        }
    }
}

// Add a directed edge. Validates against negative weights.
void add_road(City* c, int src, int dest, int w) {
    if (w < 0) {
        printf("Invalid input: negative weight\n");
        return;
    }
    c->matrix[src][dest] = w;
}

// Recursively print the path from source to destination
void print_path(int parent[], int j) {
    if (parent[j] == -1) {
        printf("%d", j);
        return;
    }
    print_path(parent, parent[j]);
    printf(" -> %d", j);
}

// Standard Dijkstra's algorithm using the adjacency matrix
void run_dijkstra(City* c, int start, int end) {
    if (start == end) {
        printf("%d\n%d\n", start, 0);
        return;
    }

    int dist[MAX_V];
    bool visited[MAX_V];
    int parent[MAX_V];

    for (int i = 0; i < c->v_count; i++) {
        dist[i] = INF;
        visited[i] = false;
        parent[i] = -1;
    }

    dist[start] = 0;

    for (int count = 0; count < c->v_count - 1; count++) {
        int min = INF, u = -1;
        for (int v = 0; v < c->v_count; v++) {
            if (!visited[v] && dist[v] <= min) {
                min = dist[v], u = v;
            }
        }

        if (u == -1 || u == end) break;
        visited[u] = true;

        for (int v = 0; v < c->v_count; v++) {
            if (!visited[v] && c->matrix[u][v] != INF &&
                dist[u] + c->matrix[u][v] < dist[v]) {
                dist[v] = dist[u] + c->matrix[u][v];
                parent[v] = u;
            }
        }
    }

    if (dist[end] == INF) {
        printf("No path found\n");
    } else {
        print_path(parent, end);
        printf("\n%d\n", dist[end]);
    }
}

// Helper function to recursively build the path array
void build_path_array(int parent[], int j, Path* p) {
    if (parent[j] == -1) {
        p->nodes[p->count++] = j;
        return;
    }
    build_path_array(parent, parent[j], p);
    p->nodes[p->count++] = j;
}

// Computes Dijkstra and returns the path data structure for the simulator
Path get_shortest_path(City* c, int start, int end) {
    Path p;
    p.count = 0;
    p.total_weight = 0;
    p.found = false;

    // Handle edge case: start and end are the same node
    if (start == end) {
        p.nodes[p.count++] = start;
        p.found = true;
        return p;
    }

    int dist[MAX_V];
    bool visited[MAX_V];
    int parent[MAX_V];

    // Initialize arrays
    for (int i = 0; i < c->v_count; i++) {
        dist[i] = INF;
        visited[i] = false;
        parent[i] = -1;
    }

    dist[start] = 0;

    // Main Dijkstra loop
    for (int count = 0; count < c->v_count - 1; count++) {
        int min = INF, u = -1;

        // Find minimum distance vertex
        for (int v = 0; v < c->v_count; v++) {
            if (!visited[v] && dist[v] <= min) {
                min = dist[v], u = v;
            }
        }

        // Break if no reachable vertex or target is reached
        if (u == -1 || u == end) break;
        visited[u] = true;

        // Update distances for adjacent vertices
        for (int v = 0; v < c->v_count; v++) {
            if (!visited[v] && c->matrix[u][v] != INF &&
                dist[u] + c->matrix[u][v] < dist[v]) {
                dist[v] = dist[u] + c->matrix[u][v];
                parent[v] = u;
            }
        }
    }

    // If a path was found, build the array
    if (dist[end] != INF) {
        p.found = true;
        p.total_weight = dist[end];
        build_path_array(parent, end, &p);
    }

    return p;
}