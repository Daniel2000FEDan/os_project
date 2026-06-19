CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
TARGET = sim
SRCS = sim.c graph.c

all: $(TARGET)

milestone3: $(TARGET)
milestone4: $(TARGET)
milestone5: $(TARGET)
milestone6: $(TARGET)
milestone7: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)