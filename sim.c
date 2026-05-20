#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "graph.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>

#define MAX_TRAVELERS 20

// State machine definitions for animation engine
typedef enum {
    STATE_IDLE,
    STATE_MOVING,
    STATE_WAITING,
    STATE_FINISHED
} AnimState;

typedef struct {
    int id;
    int start;
    int end;
    Path path;
    Color color;
    pid_t pid;
    AnimState animState;
    int pathIdx;
    double stateStartTime;
    Vector2 entityPos;
    bool active;
} Traveler;

Color travelerColors[] = { RED, PURPLE, GREEN, MAGENTA, MAROON, DARKBLUE };
int numColors = 6;


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

    int num_travelers = 0;
    Traveler travelers[MAX_TRAVELERS];

    if (fscanf(file, "%d", &num_travelers) == 1) {
        for (int i = 0; i < num_travelers && i < MAX_TRAVELERS; i++) {
            travelers[i].id = i;
            fscanf(file, "%d %d", &travelers[i].start, &travelers[i].end);

            travelers[i].path = get_shortest_path(&city, travelers[i].start, travelers[i].end);

            travelers[i].color = travelerColors[i % numColors];
            travelers[i].animState = STATE_IDLE;
            travelers[i].pathIdx = 0;
            travelers[i].active = travelers[i].path.found;
        }
    }
    fclose(file);

    //FORKING PROCESSES
    for (int i = 0; i < num_travelers; i++) {
        if (!travelers[i].active) continue;

        pid_t pid = fork();

        if (pid < 0) {
            fprintf(stderr, "Fork failed for traveler %d\n", i);
            exit(1);
        }
        else if (pid == 0) {
            // Child process
            printf("[%d] started\n", getpid());

            while (1) {
                pause();
            }
            exit(0);
        }
        else {
            // Parent process
            travelers[i].pid = pid;
        }
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

    // Set initial positions for all travelers
    for (int i = 0; i < num_travelers; i++) {
        if (travelers[i].active && travelers[i].path.count > 0) {
            travelers[i].entityPos = positions[travelers[i].path.nodes[0]];
        }
    }

    while (!WindowShouldClose()) {
        // Handle button logic
        Vector2 mousePoint = GetMousePosition();
        bool btnHover = CheckCollisionPointRec(mousePoint, playBtn);

        if (btnHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            isAnimating = !isAnimating; // Toggle animation state

            if (isAnimating) {
                for (int i = 0; i < num_travelers; i++) {
                    if (travelers[i].active && travelers[i].animState == STATE_IDLE && travelers[i].path.count > 1) {
                        travelers[i].animState = STATE_MOVING;
                        travelers[i].stateStartTime = GetTime();
                    }
                }
            }
        }

        // ANIMATION ENGINE MATH
        if (isAnimating) {
            for (int i = 0; i < num_travelers; i++) {
                if (!travelers[i].active || travelers[i].animState == STATE_FINISHED || travelers[i].animState == STATE_IDLE) continue;
                if (travelers[i].animState == STATE_WAITING) {
                    if (GetTime() - travelers[i].stateStartTime >= 1.0) {
                        travelers[i].animState = STATE_MOVING;
                        travelers[i].stateStartTime = GetTime();
                    }
                }
                else if (travelers[i].animState == STATE_MOVING) {
                    int u = travelers[i].path.nodes[travelers[i].pathIdx];
                    int v = travelers[i].path.nodes[travelers[i].pathIdx + 1];
                    int W = city.matrix[u][v];
                    double elapsed = GetTime() - travelers[i].stateStartTime;

                    int currentJump = (int)(elapsed / 0.3);

                    if (currentJump >= W) {
                        travelers[i].entityPos = positions[v];
                        travelers[i].pathIdx++;

                        if (travelers[i].pathIdx >= travelers[i].path.count - 1) {
                            travelers[i].animState = STATE_FINISHED;
                            // Send signal to terminate the child process
                            kill(travelers[i].pid, SIGTERM);
                        } else {
                            travelers[i].animState = STATE_WAITING;
                            travelers[i].stateStartTime = GetTime();
                        }
                    } else {
                        float progress = (float)currentJump / W;
                        travelers[i].entityPos.x = positions[u].x + (positions[v].x - positions[u].x) * progress;
                        travelers[i].entityPos.y = positions[u].y + (positions[v].y - positions[u].y) * progress;
                    }
                }
            }
        }


        BeginDrawing();
        ClearBackground(GetColor(0xF5F5F5FF));

        //Draw static edges
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (city.matrix[i][j] != INF && i != j) {
                    DrawArrow(positions[i], positions[j], 30, GRAY);
                    DrawWeight(positions[i], positions[j], city.matrix[i][j]);
                }
            }
        }

        // Draw static nodes
        for (int i = 0; i < n; i++) {
            DrawCircleV(positions[i], 30, SKYBLUE);
            DrawCircleLines((int)positions[i].x, (int)positions[i].y, 30, BLUE);

            const char* idText = TextFormat("%d", i);
            int fontSize = 26;
            int textWidth = MeasureText(idText, fontSize);
            DrawText(idText, (int)positions[i].x - textWidth/2, (int)positions[i].y - fontSize/2, fontSize, WHITE);
        }

        //Draw multiple travelers
        for (int i = 0; i < num_travelers; i++) {
            if (travelers[i].active) {
                DrawCircleV(travelers[i].entityPos, 15, travelers[i].color);
                DrawCircleLines((int)travelers[i].entityPos.x, (int)travelers[i].entityPos.y, 15, BLACK);
            }
        }

        // Draw Play/Stop Button
        DrawRectangleRec(playBtn, btnHover ? LIGHTGRAY : GRAY);
        DrawRectangleLinesEx(playBtn, 2, DARKGRAY);

        const char* btnText = isAnimating ? "STOP" : "PLAY";
        int btnTextWidth = MeasureText(btnText, 20);
        DrawText(btnText, playBtn.x + (playBtn.width - btnTextWidth) / 2, playBtn.y + 10, 20, BLACK);

        // Render destination message upon completion of ALL active travelers
        bool allFinished = true;
        bool hasActive = false;
        for (int i = 0; i < num_travelers; i++) {
            if (travelers[i].active) {
                hasActive = true;
                if (travelers[i].animState != STATE_FINISHED) {
                    allFinished = false;
                    break;
                }
            }
        }

        if (hasActive && allFinished) {
            const char* msg = "ALL DESTINATIONS REACHED!";
            int msgWidth = MeasureText(msg, 30);
            DrawText(msg, (screenWidth - msgWidth) / 2, 20, 30, DARKGREEN);
        }

        EndDrawing();
    }

    CloseWindow();
    // Clean up zombie child processes
    for (int i = 0; i < num_travelers; i++) {
        if (travelers[i].active) {
            waitpid(travelers[i].pid, NULL, 0);
        }
    }
    return 0;
}