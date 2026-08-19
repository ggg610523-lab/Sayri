
CC = gcc

TARGET = pulsar-assistant

CFLAGS = -Wall -Wextra -O2 \
         -I/opt/homebrew/include \
         $(shell sdl2-config --cflags)

LIBS = $(shell sdl2-config --libs) \
       -lSDL2_ttf \
       -lm

SOURCES = main.c ui.c hamburger.c sidebar.c button.c

OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all run clean
