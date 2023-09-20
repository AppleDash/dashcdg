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

static const char *g_Mp3Path;
static GLuint g_TextureId = 0;
static uint32_t g_StartTimestamp = 0;
static struct cdg_reader g_Reader;
static struct audio_state g_AudioState;

static int read_chunk_from_file(void *userData, struct subchannel_packet *outPkt) {
    FILE *fp = (FILE *) userData;

    return !feof(fp)
           && !ferror(fp)
           && (fread(outPkt, 1, sizeof(struct subchannel_packet), fp) == sizeof(struct subchannel_packet));
}

static void checkGlError(const char *where) {
    GLenum error;
    const char *errorString;

    /* Check errors from the last frame rendered */
    while ((error = glGetError()) != GL_NO_ERROR) {
        switch (error) {
            case GL_INVALID_ENUM:
                errorString = "invalid enum";
                break;
            case GL_INVALID_VALUE:
                errorString = "invalid value";
                break;
            case GL_INVALID_OPERATION:
                errorString = "invalid operation";
                break;
            case GL_STACK_OVERFLOW:
                errorString = "stack overflow";
                break;
            case GL_STACK_UNDERFLOW:
                errorString = "stack underflow";
                break;
            case GL_OUT_OF_MEMORY:
                errorString = "out of memory";
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                errorString = "invalid framebuffer operation";
                break;
            default:
                errorString = "unknown";
                break;
        }
        fprintf(stderr, "GL ERROR at %s: %d (%s)\n", where, error, errorString);
    }
}

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

static void *threadCallback(void *userData) {
    start_mp3_playback(g_Mp3Path, (struct audio_state *) userData);

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

    pthread_create(&g_AudioState.thread, NULL, threadCallback, &g_AudioState);

    glutMainLoop();

    return 0;
}
