#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_V 15
#define INF 999999

typedef struct {
    int v_count;
    int matrix[MAX_V][MAX_V];
} City;

// Initialize graph: self-loops = 0, others = INF
void init_city(City* c, int v) {
    c->v_count = v;
    for (int i = 0; i < v; i++) {
        for (int j = 0; j < v; j++) {
            c->matrix[i][j] = (i == j) ? 0 : INF;
        }
    }
}

// Add directed edge with validation
void add_road(City* c, int src, int dest, int w) {
    if (w < 0) {
        printf("Invalid input: negative weight\n");
        return;
    }
    c->matrix[src][dest] = w;
}

// Recursive function to print path: 0 -> 2 -> 5
void print_path(int parent[], int j) {
    if (parent[j] == -1) {
        printf("%d", j);
        return;
    }
    print_path(parent, parent[j]);
    printf(" -> %d", j);
}

void run_dijkstra(City* c, int start, int end) {
    // If source is same as destination
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

int main() {
    FILE* file = fopen("input.txt", "r");
    if (!file) {
        printf("Error: Could not open input.txt\n");
        return 1;
    }

    int n, m;
    if (fscanf(file, "%d %d", &n, &m) != 2) {
        fclose(file);
        return 1;
    }

    City city;
    init_city(&city, n);

    for (int i = 0; i < m; i++) {
        int u, v, w;
        if (fscanf(file, "%d %d %d", &u, &v, &w) == 3) {
            add_road(&city, u, v, w);
        }
    }

    int start, end;
    if (fscanf(file, "%d %d", &start, &end) == 2) {
        run_dijkstra(&city, start, end);
    }

    fclose(file);
    return 0;
}