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
    // Handle edge case where source and destination are the same
    if (start == end) {
        printf("%d\n%d\n", start, 0);
        return;
    }

    // Static arrays used instead of dynamic memory to prevent leaks
    int dist[MAX_V];
    bool visited[MAX_V];
    int parent[MAX_V];

    // Initialize algorithm states
    for (int i = 0; i < c->v_count; i++) {
        dist[i] = INF;
        visited[i] = false;
        parent[i] = -1;
    }

    dist[start] = 0;

    // Find shortest path for all vertices
    for (int count = 0; count < c->v_count - 1; count++) {
        int min = INF, u = -1;
        
        // Pick the minimum distance vertex from the set of unvisited vertices
        for (int v = 0; v < c->v_count; v++) {
            if (!visited[v] && dist[v] <= min) {
                min = dist[v], u = v;
            }
        }

        // If no reachable vertex is found, or we reached the target
        if (u == -1 || u == end) break;
        
        visited[u] = true;

        // Update dist value of the adjacent vertices of the picked vertex
        for (int v = 0; v < c->v_count; v++) {
            if (!visited[v] && c->matrix[u][v] != INF &&
                dist[u] + c->matrix[u][v] < dist[v]) {
                dist[v] = dist[u] + c->matrix[u][v];
                parent[v] = u;
            }
        }
    }

    // Output formatting according to requirements
    if (dist[end] == INF) {
        printf("No path found\n");
    } else {
        print_path(parent, end);
        printf("\n%d\n", dist[end]);
    }
}