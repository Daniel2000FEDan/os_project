#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "graph.h"

// Draws an arrow between two nodes, adjusting start/end points to avoid node overlap
void DrawArrow(Vector2 start, Vector2 end, float radius, Color color) {
    float angle = atan2f(end.y - start.y, end.x - start.x);
    float edgeOffset = radius + 12.0f;

    Vector2 p1 = { start.x + radius * cosf(angle), start.y + radius * sinf(angle) };
    Vector2 p2 = { end.x - edgeOffset * cosf(angle), end.y - edgeOffset * sinf(angle) };

    DrawLineEx(p1, p2, 1.8f, color);
    DrawPoly(p2, 3, 10, angle * RAD2DEG, color);
}

// Draws the edge weight text, positioned safely away from center intersections
void DrawWeight(Vector2 start, Vector2 end, int weight) {
    float ratio = 0.45f;
    Vector2 pos = {
        start.x + (end.x - start.x) * ratio,
        start.y + (end.y - start.y) * ratio
    };

    const char* label = TextFormat("%d", weight);
    int fontSize = 20;
    int textWidth = MeasureText(label, fontSize);

    DrawCircleV(pos, 14, GetColor(0xF5F5F5FF));
    DrawText(label, (int)(pos.x - textWidth / 2.0f), (int)(pos.y - fontSize / 2.0f), fontSize, DARKGRAY);
}

int main(int argc, char *argv[]) {
    // Validate command line arguments for the simulator
    if (argc != 2) {
        printf("Usage: ./sim <file_name>\n");
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        printf("Error: Could not open %s\n", argv[1]);
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

    // Read the query (start and end nodes) for the animation
    int query_start, query_end;
    bool has_query = (fscanf(file, "%d %d", &query_start, &query_end) == 2);
    fclose(file);

    // Fetch the shortest path data for the simulation
    Path path;
    if (has_query) {
        path = get_shortest_path(&city, query_start, query_end);
    }

    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Graph Simulation");
    SetTargetFPS(60);

    Vector2 positions[MAX_V];
    Vector2 center = { (float)screenWidth / 2, (float)screenHeight / 2 + 20 };
    float layoutRadius = 200.0f;

    // Calculate circular layout positions
    for (int i = 0; i < n; i++) {
        float angle = i * (360.0f / n) * DEG2RAD;
        positions[i].x = center.x + layoutRadius * cosf(angle);
        positions[i].y = center.y + layoutRadius * sinf(angle);
    }

    // UI and Animation States
    bool isAnimating = false;
    Rectangle playBtn = { 20, 20, 100, 40 };

    while (!WindowShouldClose()) {
        // Handle button logic
        Vector2 mousePoint = GetMousePosition();
        bool btnHover = CheckCollisionPointRec(mousePoint, playBtn);

        if (btnHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            isAnimating = !isAnimating; // Toggle Play/Stop state
        }

        BeginDrawing();
        ClearBackground(GetColor(0xF5F5F5FF));

        // 1. Draw static edges
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (city.matrix[i][j] != INF && i != j) {
                    DrawArrow(positions[i], positions[j], 30, GRAY);
                    DrawWeight(positions[i], positions[j], city.matrix[i][j]);
                }
            }
        }

        // 2. Draw static nodes
        for (int i = 0; i < n; i++) {
            DrawCircleV(positions[i], 30, SKYBLUE);
            DrawCircleLines((int)positions[i].x, (int)positions[i].y, 30, BLUE);

            const char* idText = TextFormat("%d", i);
            int fontSize = 26;
            int textWidth = MeasureText(idText, fontSize);
            DrawText(idText, (int)positions[i].x - textWidth/2, (int)positions[i].y - fontSize/2, fontSize, WHITE);
        }

        // 3. Draw moving entity (currently static at start node)
        if (has_query && path.found) {
            int startNode = path.nodes[0];
            DrawCircleV(positions[startNode], 15, ORANGE);
            DrawCircleLines((int)positions[startNode].x, (int)positions[startNode].y, 15, RED);
        }

        // 4. Draw Play/Stop Button
        DrawRectangleRec(playBtn, btnHover ? LIGHTGRAY : GRAY);
        DrawRectangleLinesEx(playBtn, 2, DARKGRAY);

        const char* btnText = isAnimating ? "STOP" : "PLAY";
        int btnTextWidth = MeasureText(btnText, 20);
        DrawText(btnText, playBtn.x + (playBtn.width - btnTextWidth) / 2, playBtn.y + 10, 20, BLACK);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}