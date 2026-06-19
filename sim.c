#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <string.h>
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
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>

#define MAX_TRAVELERS 20

// State machine definitions for animation engine
typedef enum {
    STATE_MOVING,
    STATE_IDLE,
    STATE_WAITING_AT_NODE,
    STATE_AT_NODE,
    STATE_FINISHED
} TravelerState;

typedef struct {
    int id;
    int start;
    int end;
    Path path;
    Color color;
    pid_t pid;
    TravelerState animState;
    int pathIdx;
    double stateStartTime;
    Vector2 entityPos;
    bool active;
    int srcNode;
    int dstNode;
    Vector2 visual_pos;      // The actual position rendered on screen
    double start_time;       // Simulation start timestamp
    double total_wait_time;  // Total accumulated waiting time in seconds
    double wait_start_tick;  // Timestamp when the traveler started waiting at the current node
} Traveler;

typedef struct {
    pid_t pid;
    int current_node;
    int next_node;
    TravelerState state;
} IPCMessage;

typedef struct {
    int id;                 // ID traveler (from 0 to num_travelers-1)
    pid_t pid;
    bool is_waiting;
    int target_node;        // Which node is the process waiting for
    long long arrival_time; // for FCFS (time in msec)
    int next_weight;        // for SJF (weight next rib)
} SharedWaitingState;

typedef struct {
    sem_t shm_mutex;                            // Shared mutex to protect this memory
    sem_t traveler_sems[MAX_TRAVELERS];         // A personal semaphore for every traveler's sleep
    SharedWaitingState travelers[MAX_TRAVELERS]; // table of states of all balls
    bool node_occupied[100];                    // Node status: true = busy, false = free (with a reserve of 100 nodes)
} SharedData;


Color travelerColors[] = { RED, PURPLE, GREEN, MAGENTA, MAROON, DARKBLUE };
int numColors = 6;
SharedData *shared_data = NULL;

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
    if (argc != 4) {
        fprintf(stderr, "Usage: %s -schd <fcfs|sjf> <file_name>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-schd") != 0) {
        fprintf(stderr, "Error: Missing -schd flag\n");
        return 1;
    }

    int scheduler_type = 0; // 1 = FCFS, 2 = SJF
    if (strcmp(argv[2], "fcfs") == 0) {
        scheduler_type = 1;
    } else if (strcmp(argv[2], "sjf") == 0) {
        scheduler_type = 2;
    } else {
        fprintf(stderr, "Error: Invalid scheduler type. Use 'fcfs' or 'sjf'.\n");
        return 1;
    }

    FILE* file = fopen(argv[3], "r");
    if (!file) {
        printf("Error: Could not open %s\n", argv[3]);
        return 1;
    }

    // Allocate anonymous shared memory for all processes
    shared_data = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_data == MAP_FAILED) {
        perror("Error: Shared memory allocation failed");
        fclose(file);
        return 1;
    }

    // Zero out the structure for safety
    memset(shared_data, 0, sizeof(SharedData));

    // Initialize the shared mutex (1 means shared between processes)
    if (sem_init(&shared_data->shm_mutex, 1, 1) < 0) {
        perror("Error: Shared mutex initialization failed");
        return 1;
    }

    // Initialize personal semaphores for each traveler with 0
    for (int i = 0; i < MAX_TRAVELERS; i++) {
        if (sem_init(&shared_data->traveler_sems[i], 1, 0) < 0) {
            perror("Error: Traveler semaphore initialization failed");
            return 1;
        }
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

    /* Creating an array to hold semaphore pointers for each node */
    sem_t **node_sems = malloc(n * sizeof(sem_t *));
    char sem_name[64];
    /* Initialization of semaphore for each node in the graph */
    for (int i = 0; i < n; i++) {
        sprintf(sem_name, "/node_sem_%d", i);
        /* Unlinking first in case of a previous crash leaving dead semaphores */
        sem_unlink(sem_name);
        /* O_CREAT creates the semaphore. 0644 are permissions.
           1 is the initial value (1 slot available in the node) */
        node_sems[i] = sem_open(sem_name, O_CREAT | O_EXCL, 0644, 1);

        if (node_sems[i] == SEM_FAILED) {
            perror("Error: Semaphore initialization failed");
            exit(1);
        }
    }

    //FORKING PROCESSES
    for (int i = 0; i < num_travelers; i++) {
        if (!travelers[i].active) continue;

        // Initialize traveler ID in shared memory before fork
        shared_data->travelers[i].id = i;
        shared_data->travelers[i].is_waiting = false;

        pid_t pid = fork();

        if (pid < 0) {
            fprintf(stderr, "Fork failed for traveler %d\n", i);
            exit(1);
        }
        else if (pid == 0) {
            close(pipe_fd[0]); // Close unused read end in child
            // Save actual child PID into shared memory
            shared_data->travelers[i].pid = getpid();

            signal(SIGUSR1, handle_start_signal);
            pause(); // Wait for start signal from parent
            for (int idx = 0; idx < travelers[i].path.count; idx++) {
                IPCMessage msg = {0};
                msg.pid = getpid();
                msg.current_node = travelers[i].path.nodes[idx];

                int weight = 1;
                if (idx == travelers[i].path.count - 1) {
                    msg.next_node = -1;
                } else {
                    msg.next_node = travelers[i].path.nodes[idx + 1];
                    weight = city.matrix[msg.current_node][msg.next_node];
                }

                /* 1. Tell parent: I am waiting outside the node */
                msg.state = STATE_WAITING_AT_NODE;
                write(pipe_fd[1], &msg, sizeof(IPCMessage));

                /* 2. Try to lock the node. If occupied, process sleeps here */
                // Get current timestamp in microseconds for FCFS
                struct timeval tv;
                gettimeofday(&tv, NULL);
                long long arrival = (long long)tv.tv_sec * 1000000LL + tv.tv_usec;

                // Lock shared memory to register waiting state safely
                sem_wait(&shared_data->shm_mutex);

                shared_data->travelers[i].is_waiting = true;
                shared_data->travelers[i].target_node = msg.current_node;
                shared_data->travelers[i].arrival_time = arrival;
                shared_data->travelers[i].next_weight = weight;

                bool can_enter = false;
                // If the node is completely vacant, this traveler can claim it immediately
                if (!shared_data->node_occupied[msg.current_node]) {
                    shared_data->node_occupied[msg.current_node] = true;
                    shared_data->travelers[i].is_waiting = false;
                    can_enter = true;
                }

                sem_post(&shared_data->shm_mutex);

                // If the node was blocked, sleep on personal semaphore until selected by scheduler
                if (!can_enter) {
                    sem_wait(&shared_data->traveler_sems[i]);
                }

                /* 3. Lock acquired! Tell parent I am inside */
                msg.state = STATE_AT_NODE;
                write(pipe_fd[1], &msg, sizeof(IPCMessage));

                /* 4. Sleep exactly 3 second inside the node */
                usleep(3000000);

                /* 5. Leave the node and unlock it for others */
                // Lock shared memory to safely evaluate the queue
                sem_wait(&shared_data->shm_mutex);

                int chosen_idx = -1;
                long long best_arrival = -1;
                int best_weight = 999999;

                // Scan for travelers waiting at this specific node
                for (int j = 0; j < num_travelers; j++) {
                    if (shared_data->travelers[j].is_waiting && shared_data->travelers[j].target_node == msg.current_node) {
                        if (scheduler_type == 1) { // FCFS logic: find earliest arrival time
                            if (chosen_idx == -1 || shared_data->travelers[j].arrival_time < best_arrival) {
                                best_arrival = shared_data->travelers[j].arrival_time;
                                chosen_idx = j;
                            }
                        } else if (scheduler_type == 2) { // SJF logic: find shortest next edge weight
                            if (chosen_idx == -1 || shared_data->travelers[j].next_weight < best_weight) {
                                best_weight = shared_data->travelers[j].next_weight;
                                chosen_idx = j;
                            }
                        }
                    }
                }

                if (chosen_idx != -1) {
                    // Pass the node lock directly to the chosen traveler and wake them up
                    shared_data->travelers[chosen_idx].is_waiting = false;
                    sem_post(&shared_data->traveler_sems[chosen_idx]);
                } else {
                    // No concurrent workers waiting, set node status to vacant
                    shared_data->node_occupied[msg.current_node] = false;
                }

                sem_post(&shared_data->shm_mutex);

                /* 6. If not destination, travel to the next node */
                if (msg.next_node != -1) {
                    msg.state = STATE_MOVING;
                    write(pipe_fd[1], &msg, sizeof(IPCMessage));
                    usleep(weight * 250000);
                } else {
                    /* Reached destination */
                    msg.state = STATE_FINISHED;
                    write(pipe_fd[1], &msg, sizeof(IPCMessage));
                }
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
            // Initialize visual position to prevent teleportation glitches on start
            travelers[i].visual_pos = travelers[i].entityPos;
        }
    }

    while (!WindowShouldClose()) {
        IPCMessage msg = {0};
        // Read all available messages from the pipe for the current frame
        while (read(pipe_fd[0], &msg, sizeof(IPCMessage)) > 0) {
            for (int i = 0; i < num_travelers; i++) {
                if (travelers[i].active && travelers[i].pid == msg.pid) {
                    /* Handle traveler reaching their final destination */
                    if (msg.state == STATE_FINISHED) {
                        travelers[i].animState = STATE_FINISHED;
                        travelers[i].entityPos = positions[msg.current_node];
                        printf("[PID=%d] FINISHED at node %d\n", msg.pid, msg.current_node);
                    }
                    /* Handle traveler waiting outside a busy node (waiting for semaphore lock) */
                    else if (msg.state == STATE_WAITING_AT_NODE) {
                        // FIX: use 'i' instead of msg.traveler_id
                        travelers[i].wait_start_tick = GetTime();
                        travelers[i].animState = STATE_WAITING_AT_NODE;
                        travelers[i].dstNode = msg.current_node; /* Save target node for drawing */
                        travelers[i].entityPos = positions[msg.current_node];
                        printf("[PID=%d] WAITING outside node %d...\n", msg.pid, msg.current_node);
                    }
                    /* Handle traveler successfully entering the node (semaphore lock acquired) */
                    else if (msg.state == STATE_AT_NODE) {
                        // FIX: Calculate accumulated waiting time when entering the node
                        if (travelers[i].wait_start_tick > 0.0) {
                            travelers[i].total_wait_time += (GetTime() - travelers[i].wait_start_tick);
                            travelers[i].wait_start_tick = 0.0;
                        }
                        travelers[i].animState = STATE_AT_NODE;
                        travelers[i].entityPos = positions[msg.current_node];
                        printf("[PID=%d] ENTERED node %d\n", msg.pid, msg.current_node);
                    }
                    /* Handle traveler leaving the node and moving along the edge */
                    else if (msg.state == STATE_MOVING) {
                        travelers[i].animState = STATE_MOVING;
                        travelers[i].srcNode = msg.current_node;
                        travelers[i].dstNode = msg.next_node;
                        travelers[i].stateStartTime = GetTime();
                        printf("[PID=%d] MOVING from %d to %d\n", msg.pid, msg.current_node, msg.next_node);
                    }
                    fflush(stdout);
                    break;
                }
            }
        }
        // Handle button logic
        Vector2 mousePoint = GetMousePosition();
        bool btnHover = CheckCollisionPointRec(mousePoint, playBtn);

        if (btnHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            isAnimating = !isAnimating; // Toggle animation state

            // Start metrics tracking on the very first simulation launch
            if (isAnimating) {
                double current_time = GetTime();
                for (int i = 0; i < num_travelers; i++) {
                    if (travelers[i].active && travelers[i].start_time == 0.0) {
                        travelers[i].start_time = current_time;
                        travelers[i].total_wait_time = 0.0;
                        travelers[i].wait_start_tick = 0.0;
                    }
                }
            }

            for (int i = 0; i < num_travelers; i++) {
                if (travelers[i].active) {
                    if (isAnimating) {
                        /* Wake up idle travelers or resume paused ones */
                        if (travelers[i].animState == STATE_IDLE && travelers[i].path.count > 1) {
                            kill(travelers[i].pid, SIGUSR1);
                        } else if (travelers[i].animState != STATE_FINISHED) {
                            kill(travelers[i].pid, SIGCONT);
                        }
                    } else {
                        /* Freeze processes via OS signals when stopped */
                        if (travelers[i].animState != STATE_IDLE && travelers[i].animState != STATE_FINISHED) {
                            kill(travelers[i].pid, SIGSTOP);
                        }
                    }
                }
            }
        }
        /* Adjust start times while paused to prevent visual teleportation */
        if (!isAnimating) {
            for (int i = 0; i < num_travelers; i++) {
                if (travelers[i].active && travelers[i].animState == STATE_MOVING) {
                    travelers[i].stateStartTime += GetFrameTime();
                }
            }
        }

        // Discrete frame-by-frame animation update
        for (int i = 0; i < num_travelers; i++) {
            if (travelers[i].active && travelers[i].animState == STATE_MOVING) {
                double elapsed = GetTime() - travelers[i].stateStartTime;

                // Retrieve the actual edge weight (which equals the number of discrete steps)
                int weight = city.matrix[travelers[i].srcNode][travelers[i].dstNode];

                // slow move by line
                float total_time = weight * 0.25f;
                float t = (float)elapsed / total_time;
                if (t > 1.0f) t = 1.0f;

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

        /* Draw multiple travelers with orbit logic for waiting states */
        int waiting_counts[MAX_V] = {0};
        int waiting_index[MAX_TRAVELERS] = {0};

        /* First pass: count how many travelers are waiting at each node */
        for (int i = 0; i < num_travelers; i++) {
            if (travelers[i].active && travelers[i].animState == STATE_WAITING_AT_NODE) {
                int node = travelers[i].dstNode;
                waiting_index[i] = waiting_counts[node];
                waiting_counts[node]++;
            }
        }

        /* Second pass: render the travelers */
        for (int i = 0; i < num_travelers; i++) {
            if (travelers[i].active && travelers[i].animState != STATE_FINISHED) {
                Vector2 drawPos = travelers[i].entityPos;
                Color drawColor = travelers[i].color;

                /* If waiting, calculate position on an orbit around the target node */
                if (travelers[i].animState == STATE_WAITING_AT_NODE) {
                    int node = travelers[i].dstNode;
                    int total_waiting = waiting_counts[node];
                    int idx = waiting_index[i];

                    /* Distribute evenly in a circle (radius 45 is slightly larger than node radius 30) */
                    float angle = idx * (2 * PI / total_waiting);
                    float orbitRadius = 45.0f;

                    drawPos.x = positions[node].x + orbitRadius * cosf(angle);
                    drawPos.y = positions[node].y + orbitRadius * sinf(angle);

                    /* Fade color to visually indicate waiting state */
                    drawColor = Fade(drawColor, 0.4f);
                }


               // Smoothly interpolate visual position toward the calculated draw target
               float smoothingSpeed = 12.0f;
               travelers[i].visual_pos.x += (drawPos.x - travelers[i].visual_pos.x) * smoothingSpeed * GetFrameTime();
               travelers[i].visual_pos.y += (drawPos.y - travelers[i].visual_pos.y) * smoothingSpeed * GetFrameTime();

                /* Draw the traveler */
                DrawCircleV(travelers[i].visual_pos, 15, drawColor);
                DrawCircleLines((int)travelers[i].visual_pos.x, (int)travelers[i].visual_pos.y, 15, BLACK);

                /* Draw a visual warning icon "!" if waiting */
                if (travelers[i].animState == STATE_WAITING_AT_NODE) {
                DrawText("!", (int)travelers[i].visual_pos.x - 3, (int)travelers[i].visual_pos.y - 10, 20, RED);
                }
            }
        }

        // Draw Play/Stop Button
        DrawRectangleRec(playBtn, btnHover ? LIGHTGRAY : GRAY);
        DrawRectangleLinesEx(playBtn, 2, DARKGRAY);

        const char* btnText = isAnimating ? "STOP" : "PLAY";
        int btnTextWidth = MeasureText(btnText, 20);
        DrawText(btnText, playBtn.x + (playBtn.width - btnTextWidth) / 2, playBtn.y + 10, 20, BLACK);

        // Display the currently active scheduler mode on screen
        const char* modeText = (scheduler_type == 1) ? "Scheduler: FCFS" : "Scheduler: SJF";
        DrawText(modeText, 20, 75, 20, DARKGRAY);

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

    // Display final performance metrics for each traveler
    printf("\nSIMULATION STATISTICS\n");
    for (int i = 0; i < num_travelers; i++) {
        if (travelers[i].active) {
            double turnaround_time = GetTime() - travelers[i].start_time;
            printf("Traveler %d (PID: %d):\n", travelers[i].id, travelers[i].pid);
            printf("  - Turnaround Time: %.2f seconds\n", turnaround_time);
            printf("  - Total Waiting Time: %.2f seconds\n", travelers[i].total_wait_time);
            printf("\n");
        }
    }
    printf("\n");


    CloseWindow();
    // Clean up zombie child processes
    for (int i = 0; i < num_travelers; i++) {
        if (travelers[i].active) {
            waitpid(travelers[i].pid, NULL, 0);
        }
    }
    // Clean up semaphores before exiting
    for (int i = 0; i < n; i++) {
        sprintf(sem_name, "/node_sem_%d", i);
        sem_close(node_sems[i]);
        sem_unlink(sem_name);
    }
    free(node_sems);
    return 0;
}