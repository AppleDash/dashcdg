#include <stdio.h>
#include <string.h>
#include <GL/glew.h>
#include <GL/glut.h>

#include "cdg.h"
#include "audio.h"

struct {
    GLuint id;
    GLint colorTableLocation;
    GLint framebufferLocation;
} g_Shader;

static GLuint g_TextureId = 0;
static uint32_t g_StartTimestamp = 0;
static struct cdg_reader g_Reader;
static struct audio_state g_AudioState;

struct mp3_playback_data {
    char *path;
    struct audio_state *state;
} g_PlaybackData;

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

    if (g_StartTimestamp == 0) {
        g_StartTimestamp = ATOMIC_INT_GET(g_AudioState.timestamp);
    } else {
        ms = ATOMIC_INT_GET(g_AudioState.timestamp) - g_StartTimestamp;
        if (cdg_reader_advance_to(&g_Reader, ms)) {
            glUniform1i(g_Shader.framebufferLocation, 0);
            glUniform1iv(g_Shader.colorTableLocation, 16, g_Reader.state.color_table);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 300, 216, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_Reader.state.framebuffer);

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

static void *mp3_player_thread_callback(void *userData) {
    struct mp3_playback_data *data = (struct mp3_playback_data *) userData;

    if (!play_mp3(data->path, data->state)) {
        fprintf(stderr, "failed to start MP3 playback\n");
        return NULL;
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <cdg> <mp3>\n", argv[0]);
        return 1;
    }

    // Set up the CDG reader
    memset(&g_Reader, 0, sizeof(struct cdg_reader));

    if (!cdg_reader_load_file(&g_Reader, argv[1])) {
        fprintf(stderr, "failed to open file\n");
        return 1;
    }

    cdg_reader_build_keyframe_list(&g_Reader);

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

    // Set up the MP3 player
    g_PlaybackData.path = argv[2];
    g_PlaybackData.state = &g_AudioState;

    pthread_create(&g_AudioState.thread, NULL, mp3_player_thread_callback, &g_PlaybackData);

    // Start rendering
    glutMainLoop();

    return 0;
}
