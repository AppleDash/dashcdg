#include <stdio.h>

#include <GL/glew.h>
#include <GL/glut.h>
#include <string.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "cdg.h"
#include "audio.h"

#define ARRAY_INDEX(X, Y) (((Y) * 300) + (X))

void process_cdg_insn(struct subchannel_packet *pkt);
int testMain(int argc, char *argv[]);

// return: 0 -> eof, 1 -> read an instruction, 2 -> read an empty subchannel packet
typedef int (*cdg_reader_read_callback)(void *userData, struct subchannel_packet *outPkt);

struct cdg_snapshot {
    uint32_t timestamp;      // subchannel packet count
    uint8_t color_table[16];
    uint8_t clear_color;
};

struct cdg_state {
    uint32_t subchannel_packet_count;
    uint16_t color_table[16];
    uint8_t framebuffer[300 * 216];
};

struct cdg_reader {
    int eof;
    struct cdg_state state;
    void *userData;
    // called when the cdg reader needs to read a CDG instruction
    cdg_reader_read_callback read_callback;
};

static GLuint g_TextureId = 0;
static uint8_t g_TexBuffer[300 * 216 * 3];
static unsigned long g_Start = 0;
static struct cdg_reader g_Reader;
static struct audio_state g_AudioState;

// Returns an integer representing the number of milliseconds elapsed since the start of the CDG stream.
inline uint32_t cdg_state_get_time_elapsed(struct cdg_state *state) {
    // 300 packets per second
    return (state->subchannel_packet_count * 1000) / 300;
}

// Returns: 1 if we need to update the framebuffer
int cdg_state_process_insn(struct cdg_state *state, struct subchannel_packet *pkt) {
    uint8_t code;
    struct cdg_insn *insn;

    state->subchannel_packet_count++;

    // not a CDG packet
    if ((pkt->command & 0b111111) != 9) {
        return 0;
    }

    code = pkt->instruction;
    insn = (struct cdg_insn *) pkt->data;

    //process_cdg_insn(pkt);

    switch (code) {
        // Load colors 0-7
        case CDG_INSN_LOAD_COLOR_TABLE_00: {
            struct cdg_insn_load_color_table *insn_load_color_table = (struct cdg_insn_load_color_table *) insn;

            for (int i = 0; i < 8; i++) {
                state->color_table[i] = ntohs(insn_load_color_table->spec[i] & 0x3F3F);
            }

            return 1;
        }
        break;

        // Load colors 8-15
        case CDG_INSN_LOAD_COLOR_TABLE_08: {
            struct cdg_insn_load_color_table *insn_load_color_table = (struct cdg_insn_load_color_table *) insn;

            for (int i = 0; i < 8; i++) {
                state->color_table[i + 8] = ntohs(insn_load_color_table->spec[i] & 0x3F3F);
            }

            return 1;
        }
        break;

        // Clear the screen
        case CDG_INSN_MEMORY_PRESET: {
            struct cdg_insn_memory_preset *insn_memory_preset = (struct cdg_insn_memory_preset *) insn;

            // The repeat code is incremented each time the same command is sent.
            // This is to ensure the screen is cleared in a potentially unreliable stream.
            // Since we're reading from a file, we can just check if the repeat code is 0 and only do this once.
            if (insn_memory_preset->repeat == 0) {
                memset(state->framebuffer, insn_memory_preset->color, 300 * 216);
            }

            return 1;
        }
        break;

        case CDG_INSN_BORDER_PRESET: {
            printf("BORDER\n");
            //The border area is the area contained with a
            //rectangle defined by (0,0,300,216) minus the interior pixels which are contained
            //within a rectangle defined by (6,12,294,204).
            struct cdg_insn_border_preset *insn_border_preset = (struct cdg_insn_border_preset *) insn;

            for (int x = 0; x < 300; x++) {
                if (x > 6 && x < 294) {
                    continue;
                }

                for (int y = 0; y < 216; y++) {
                    if (y > 12 && y < 204) {
                        continue;
                    }

                    state->framebuffer[ARRAY_INDEX(x, y)] = insn_border_preset->color;
                }
            }
            return 1;
        }
        break;

        // Copy a block of pixels into the framebuffer
        case CDG_INSN_TILE_BLOCK:
        case CDG_INSN_TILE_BLOCK_XOR: {
            struct cdg_insn_tile_block *insn_tile_block = (struct cdg_insn_tile_block *) insn;
            int isXor = code == CDG_INSN_TILE_BLOCK_XOR;

            size_t startRow;
            size_t startCol;
            // Row and Column describe the position of the tile in tile coordinate space.  To
            // convert to pixels, multiply row by 12, and column by 6.
            startRow = (insn_tile_block->row & 0x1F) * 12;
            startCol = (insn_tile_block->column & 0x3F) * 6;

            // tilePixels[] contains the actual bit values for the tile, six pixels per byte.
            // The uppermost valid bit of each byte (0x20) contains the left-most pixel of each
            // scanline of the tile.
            // pretty sure it's 6 rows of 12 pixels, so 2 bytes per row

            for (int i = 0; i < 12; i++) {
                uint8_t tilePixels = insn_tile_block->pixels[i] & 0x3F;

                for (int j = 0; j < 6; j++) {
                    uint8_t pixel = (tilePixels >> (5 - j)) & 1;
                    uint8_t color = (pixel ? insn_tile_block->color_1 : insn_tile_block->color_0) & 0xF;

                    if (isXor) {
                        state->framebuffer[ARRAY_INDEX(startCol + j, startRow + i)] ^= color;
                    } else {
                        state->framebuffer[ARRAY_INDEX(startCol + j, startRow + i)] = color;
                    }
                }
            }

            return 1;
        }
        break;

        case CDG_INSN_SCROLL_PRESET:
        case CDG_INSN_SCROLL_COPY: {
            // TODO: implement - I haven't seen this in any CDG files yet
            printf("they be doing scrolling\n");
            struct cdg_insn_scroll *insn_scroll = (struct cdg_insn_scroll *) insn;
            uint8_t color = insn_scroll->color;
            uint8_t h_scroll = insn_scroll->h_scroll;
            uint8_t v_scroll = insn_scroll->v_scroll;
        }
        break;
        default:
            printf("unexpected insn: %d\n", code);
    }

    return 0;
}

// Returns: 1 if we need to update the framebuffer
int cdg_reader_advance_to(struct cdg_reader *reader, uint32_t timestamp) {
    struct subchannel_packet insn;
    int needsUpdate = 0;

    while (cdg_state_get_time_elapsed(&reader->state) < timestamp) {
        if (!reader->read_callback(reader->userData, &insn)) {
            // End of CDG stream
            reader->eof = 1;
            break;
        }

        // Intentionally using |= here so that cdg_state_process_insn() is always called
        needsUpdate |= cdg_state_process_insn(&reader->state, &insn);
    }

    return needsUpdate;
}


void cdg_state_fb_to_8_bit_color(struct cdg_state *state, uint8_t out[300 * 216 * 3]) {
    /*
     * Each colorSpec value can be converted to RGB using the following diagram:
     * [---high byte---]   [---low byte----]
     *  7 6 5 4 3 2 1 0     7 6 5 4 3 2 1 0
     *  X X r r r r g g     X X g g b b b b
     */
    size_t idx;

    for (int y = 0; y < 216; y++) {
        for (int x = 0; x < 300; x++) {
            idx = ARRAY_INDEX(x, y);
            uint16_t color = state->color_table[state->framebuffer[idx]];
            uint8_t r = (color & 0x3C00) >> 10;
            uint8_t g = ((color & 0x300) >> 6) | ((color & 0x30) >> 4);
            uint8_t b = (color & 0xF);

            out[(idx * 3) + 0] = r * 16;
            out[(idx * 3) + 1] = g * 16;
            out[(idx * 3) + 2] = b * 16;
        }
    }
}

unsigned long millis_since_epoch() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

uint32_t g_LastTimestamp = 0;
void display(void) {
    uint32_t ms;

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_TextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    if (g_LastTimestamp == 0) {
        g_LastTimestamp = ATOMIC_INT_GET(g_AudioState.timestamp);
    } else {
        ms = ATOMIC_INT_GET(g_AudioState.timestamp) - g_LastTimestamp;
        if (cdg_reader_advance_to(&g_Reader, ms)) {
            cdg_state_fb_to_8_bit_color(&g_Reader.state, g_TexBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 300, 216, 0, GL_RGB, GL_UNSIGNED_BYTE, g_TexBuffer);
        }
    }

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(1, 0); glVertex2f(300, 0);
    glTexCoord2f(1, 1); glVertex2f(300, 216);
    glTexCoord2f(0, 1); glVertex2f(0, 216);
    glEnd();

    glFlush();
    glutSwapBuffers();
    glutPostRedisplay();
}

int read_chunk_from_file(void *userData, struct subchannel_packet *outPkt) {
    FILE *fp = (FILE *) userData;

    return !feof(fp)
            && !ferror(fp)
            && (fread(outPkt, 1, sizeof(struct subchannel_packet), fp) == sizeof(struct subchannel_packet));
}

void resizeCallback(int width, int height) {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glViewport(0, 0, width, height);
    glOrtho(0, 300, 216, 0, 0.0, 100.0);

    if (g_TextureId == 0) {
        glGenTextures(1, &g_TextureId);
    }
}

static const char *mp3Path;

void *threadCallback(void *userData) {
    struct audio_state *state = (struct audio_state *) userData;

    start_mp3_playback(mp3Path, state);


    return NULL;
}

int main(int argc, char *argv[]) {
    FILE *fp;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <cdg> <mp3>\n", argv[0]);
        return 1;
    }

    mp3Path = argv[2];

    memset(&g_Reader, 0, sizeof(struct cdg_reader));
    fp = fopen(argv[1], "r");

    if (!fp) {
        fprintf(stderr, "failed to open file\n");
        return 1;
    }

    g_Reader.userData = fp;
    g_Reader.read_callback = read_chunk_from_file;

    g_Start = millis_since_epoch();

    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(300*4, 216*4);

    glutCreateWindow("CDG");

    glewExperimental = GL_TRUE;
    glewInit();

    glutDisplayFunc(display);
    glutReshapeFunc(resizeCallback);

    pthread_create(&g_AudioState.thread, NULL, threadCallback, &g_AudioState);

    glutMainLoop();

    return 0;
}
