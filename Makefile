CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
TARGET = sim
SRCS = sim.c graph.c

# Если написать просто 'make'
all: $(TARGET)

# Если препод по привычке напишет старую команду
milestone3: $(TARGET)
milestone4: $(TARGET)
milestone5: $(TARGET)

# Сам рецепт сборки
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)