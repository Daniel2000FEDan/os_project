#include <stdio.h>
#include "graph.h"

int main(int argc, char *argv[]) {
    // Require file name from command line arguments
    if (argc != 2) {
        printf("Usage: ./dijkstra <file_name>\n");
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        printf("Error: Could not open %s\n", argv[1]);
        return 1;
    }

    int n, m;
    // Read number of vertices (n) and edges (m)
    if (fscanf(file, "%d %d", &n, &m) != 2) {
        fclose(file);
        return 1;
    }

    City city;
    init_city(&city, n);

    // Read all edges
    for (int i = 0; i < m; i++) {
        int u, v, w;
        if (fscanf(file, "%d %d %d", &u, &v, &w) == 3) {
            add_road(&city, u, v, w);
        }
    }

    // Read the query for Dijkstra
    int start, end;
    if (fscanf(file, "%d %d", &start, &end) == 2) {
        run_dijkstra(&city, start, end);
    }

    fclose(file);
    return 0;
}