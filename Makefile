CC      := gcc
CFLAGS  := -Wall -Wextra -Wno-cpp -std=c99 -pedantic -D_FORTIFY_SOURCE=2 -Iinc/
LDFLAGS := -lGL -lGLEW -lglut -lportaudio
OBJECTS := obj/shaders.o obj/util.o obj/audio.o obj/player.o obj/cdg.o
HEADERS := inc/shaders.h inc/util.h inc/audio.h inc/cdg.h
BINARY  := cdg

all: CFLAGS += -O2
all: $(BINARY)

debug: CFLAGS += -DDEBUG -g
debug: $(BINARY)

$(BINARY): $(OBJECTS) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS)
	rm -f $(BINARY)
