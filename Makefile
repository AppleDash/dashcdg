CC      := gcc
CFLAGS  := -Iinc/ -Wall -Wextra -Wno-cpp -std=c99 -pedantic -D_FORTIFY_SOURCE=2 -O1 -g
LDFLAGS := -lGL -lGLEW -lglut -lportaudio
OBJECTS := obj/util.o obj/audio.o obj/render.o obj/cdg.o
BINARY  := cdg

all: $(BINARY)

$(BINARY): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS)
	rm -f $(BINARY)
