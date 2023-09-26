#include <stdio.h>
#include <string.h>
#include <GL/glew.h>
#include <GL/glut.h>
#include <assert.h>

#include "cdg.h"
#include "audio.h"

struct {
    GLuint id;
    GLint colorTableLocation;
    GLint framebufferLocation;
} g_Shader;

static GLuint g_TextureId = 0;
static struct cdg_reader *g_Reader;
static struct audio_state *g_AudioState;

void display(void) {
    uint32_t ms;

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_TextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glUseProgram(g_Shader.id);
    ms = ATOMIC_INT_GET(g_AudioState->timestamp);

    if (cdg_reader_seek_bidirectional(g_Reader, MS_TO_CDG_FRAME_COUNT(ms))) {
        glUniform1i(g_Shader.framebufferLocation, 0);
        glUniform1iv(g_Shader.colorTableLocation, 16, g_Reader->state.color_table);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 300, 216, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_Reader->state.framebuffer);
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

static void seek(uint32_t ms) {
    audio_state_seek(g_AudioState, ms);
}

void specialKeyboardCallback(int key, int x, int y) {
    UNUSED(x); UNUSED(y);

    uint32_t currentPos;

    currentPos = audio_state_get_pos(g_AudioState);

    switch (key) {
        case GLUT_KEY_RIGHT:
            seek(currentPos + 1000);
            break;
        case GLUT_KEY_LEFT:
            seek(currentPos - 1000);
            break;
        default:
            // Do nothing
            break;
    }
}

// This will be run from the audio playback thread.
static void *mp3_player_thread_callback(void *userData) {
    assert(userData != NULL);

    if (!audio_state_load_file(g_AudioState, (char *) userData, NULL)) {
        fprintf(stderr, "failed to load MP3 file\n");
        return (void *) 0;
    }

    if (!audio_do_playback(g_AudioState)) {
        fprintf(stderr, "failed to start MP3 playback\n");
        return (void *) 0;
    }

    return (void *) 1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <cdg> <mp3>\n", argv[0]);
        return 1;
    }

    // Set up the CDG reader
    g_Reader = cdg_reader_new();

    if (!cdg_reader_load_file(g_Reader, argv[1])) {
        fprintf(stderr, "failed to open file\n");
        return 1;
    }

    cdg_reader_build_keyframe_list(g_Reader);

    // Set up OpenGL
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(300*4, 216*4);

    glutCreateWindow("CDG");

    glewExperimental = GL_TRUE;
    glewInit();

    if ((g_Shader.id = load_shader_program("cdg")) == 0) {
        fprintf(stderr, "failed to load shader program\n");
        return 1;
    }

    if ((g_Shader.colorTableLocation = glGetUniformLocation(g_Shader.id, "cdgColorMap")) == -1) {
        fprintf(stderr, "failed to get color table uniform location\n");
        return 1;
    }

    if ((g_Shader.framebufferLocation = glGetUniformLocation(g_Shader.id, "cdgFramebuffer")) == -1) {
        fprintf(stderr, "failed to get framebuffer uniform location\n");
        return 1;
    }

    glutDisplayFunc(display);
    glutReshapeFunc(resizeCallback);
    glutSpecialFunc(specialKeyboardCallback);

    // Set up the MP3 player
    g_AudioState = audio_state_new();
    pthread_create(&g_AudioState->thread, NULL, mp3_player_thread_callback, argv[2]);

    // Start rendering
    glutMainLoop();

    cdg_reader_free(g_Reader);
    audio_state_free(g_AudioState);

    return 0;
}
