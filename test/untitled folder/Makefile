CC = cc
CFLAGS = -I/opt/homebrew/include/SDL2 -D_THREAD_SAFE -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter
LDLIBS = -lSDL2_ttf -L/opt/homebrew/lib -lSDL2main -lSDL2 -Wl,-framework,Cocoa

OBJS = main.o glcore.o util.o orb.o ripple.o button.o canvas.o

reacticx_demo: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

%.o: %.c glcore.h util.h canvas.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f reacticx_demo *.o shot_*.ppm

.PHONY: clean