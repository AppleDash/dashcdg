#include <stdio.h>
#include <string.h>
#include <GL/glew.h>
#include <GL/glut.h>

#include "cdg.h"
#include "audio.h"


void process_cdg_insn(struct subchannel_packet *pkt);

static const char *g_Mp3Path;
static GLuint g_TextureId = 0;
static uint8_t g_TexBuffer[300 * 216 * 3];
static uint32_t g_StartTimestamp = 0;
static struct cdg_reader g_Reader;
static struct audio_state g_AudioState;

static int read_chunk_from_file(void *userData, struct subchannel_packet *outPkt) {
    FILE *fp = (FILE *) userData;

    return !feof(fp)
           && !ferror(fp)
           && (fread(outPkt, 1, sizeof(struct subchannel_packet), fp) == sizeof(struct subchannel_packet));
}

static void cdg_state_fb_to_8_bit_color(struct cdg_state *state, uint8_t out[300 * 216 * 3]) {
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

    if (g_StartTimestamp == 0) {
        g_StartTimestamp = ATOMIC_INT_GET(g_AudioState.timestamp);
    } else {
        ms = ATOMIC_INT_GET(g_AudioState.timestamp) - g_StartTimestamp;
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


static void *threadCallback(void *userData) {
    struct audio_state *state = (struct audio_state *) userData;

    start_mp3_playback(g_Mp3Path, state);


    return NULL;
}

int main(int argc, char *argv[]) {
    FILE *fp;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <cdg> <mp3>\n", argv[0]);
        return 1;
    }

    g_Mp3Path = argv[2];

    memset(&g_Reader, 0, sizeof(struct cdg_reader));
    fp = fopen(argv[1], "r");

    if (!fp) {
        fprintf(stderr, "failed to open file\n");
        return 1;
    }

    g_Reader.userData = fp;
    g_Reader.read_callback = read_chunk_from_file;

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
