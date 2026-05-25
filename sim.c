#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <fcntl.h>
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
    int srcNode;
    int dstNode;
} Traveler;

typedef struct {
    pid_t pid;
    int current_node;
    int next_node;
    bool is_finished;
} IPCMessage;

Color travelerColors[] = { RED, PURPLE, GREEN, MAGENTA, MAROON, DARKBLUE };
int numColors = 6;

void handle_start_signal(int sig) {
    (void)sig;
}

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
    int pipe_fd[2];
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

    // Initialize IPC pipe
    if (pipe(pipe_fd) < 0) {
        perror("Error: pipe failed");
        exit(1);
    }

    // Set the read end of the pipe to non-blocking mode
    int flags = fcntl(pipe_fd[0], F_GETFL, 0);
    fcntl(pipe_fd[0], F_SETFL, flags | O_NONBLOCK);

    //FORKING PROCESSES
    for (int i = 0; i < num_travelers; i++) {
        if (!travelers[i].active) continue;

        pid_t pid = fork();

        if (pid < 0) {
            fprintf(stderr, "Fork failed for traveler %d\n", i);
            exit(1);
        }
        else if (pid == 0) {
            close(pipe_fd[0]); // Close unused read end in child

            signal(SIGUSR1, handle_start_signal);
            pause(); // Wait for start signal from parent
            for (int idx = 0; idx < travelers[i].path.count; idx++) {
                IPCMessage msg = {0};
                msg.pid = getpid();
                msg.current_node = travelers[i].path.nodes[idx];

                int weight = 1; // default weight for finish
                if (idx == travelers[i].path.count - 1) {
                    msg.next_node = -1;
                    msg.is_finished = true;
                } else {
                    msg.next_node = travelers[i].path.nodes[idx + 1];
                    msg.is_finished = false;
                    // taking real weight from matrix
                    weight = city.matrix[msg.current_node][msg.next_node];
                }

                write(pipe_fd[1], &msg, sizeof(IPCMessage));

                // sleep depend on weight
                usleep(weight * 250000);
            }
            exit(0);
        }
        else {
            // Parent process
            travelers[i].pid = pid;
        }
    }
    close(pipe_fd[1]); // Close unused write end in parent

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
        IPCMessage msg = {0};
        // Read all available messages from the pipe for the current frame
        while (read(pipe_fd[0], &msg, sizeof(IPCMessage)) > 0) {
            for (int i = 0; i < num_travelers; i++) {
                if (travelers[i].active && travelers[i].pid == msg.pid) {
                    if (msg.is_finished) {
                        travelers[i].animState = STATE_FINISHED;
                        travelers[i].entityPos = positions[msg.current_node];
                        printf("[PID=%d] finished\n", msg.pid);
                    } else if (msg.next_node == -1) {
                        printf("[PID=%d] arrived at node %d | DESTINATION\n", msg.pid, msg.current_node);
                    } else {
                        printf("[PID=%d] arrived at node %d | next node: %d\n", msg.pid, msg.current_node, msg.next_node);

                        travelers[i].animState = STATE_MOVING;
                        travelers[i].srcNode = msg.current_node;
                        travelers[i].dstNode = msg.next_node;
                        travelers[i].stateStartTime = GetTime();
                    }
                    fflush(stdout);
                    break;
                }
            }
        }
        // Handle button logic
        Vector2 mousePoint = GetMousePosition();
        bool btnHover = CheckCollisionPointRec(mousePoint, playBtn);

        if (btnHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            isAnimating = !isAnimating; // Toggle animation state
            if (isAnimating) {
                for (int i = 0; i < num_travelers; i++) {
                    if (travelers[i].active && travelers[i].animState == STATE_IDLE && travelers[i].path.count > 1) {
                        kill(travelers[i].pid, SIGUSR1); // Wake up this specific child process
                    }
                }
            }
        }

        // Discrete frame-by-frame animation update
        for (int i = 0; i < num_travelers; i++) {
            if (travelers[i].active && travelers[i].animState == STATE_MOVING) {
                double elapsed = GetTime() - travelers[i].stateStartTime;

                // Retrieve the actual edge weight (which equals the number of discrete steps)
                int weight = city.matrix[travelers[i].srcNode][travelers[i].dstNode];

                // Calculate the current step (one step occurs every 0.25 seconds)
                int current_step = (int)(elapsed / 0.25f);

                // Clamp the current step to prevent overshooting the destination node
                if (current_step > weight) current_step = weight;

                // Calculate the discrete interpolation factor (e.g., 0.0 -> 0.5 -> 1.0 for weight 2)
                float t = (float)current_step / weight;

                Vector2 startPos = positions[travelers[i].srcNode];
                Vector2 endPos = positions[travelers[i].dstNode];

                // Update the entity position to the specific fractional point along the edge
                travelers[i].entityPos.x = startPos.x + (endPos.x - startPos.x) * t;
                travelers[i].entityPos.y = startPos.y + (endPos.y - startPos.y) * t;
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