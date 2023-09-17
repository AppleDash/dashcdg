CC      := gcc
CFLAGS  := -Wall -Wextra -Wno-cpp -std=c99 -pedantic -D_FORTIFY_SOURCE=2 -O1 -g
LDFLAGS := -lGL -lGLEW -lglut -lpng
OBJECTS := util.o render.o cdg.o
BINARY  := cdg

all: $(BINARY)

$(BINARY): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS)
	rm -f $(BINARY)
