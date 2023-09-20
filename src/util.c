#include "util.h"
#include <stdio.h>
#include <malloc.h>

int read_file(const char *path, char **buf, unsigned long *size) {
    FILE *fp;

    fp = fopen(path, "r");

    if (!fp) {
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    *buf = (char *) malloc((*size) + 1);
    if (fread(*buf, 1, *size, fp) != *size) {
        free(*buf);
        fclose(fp);
        return 0;
    }

    (*buf)[*size] = '\0';

    fclose(fp);

    return 1;
}

GLuint load_shader_program(const char *name) {
    GLint status;
    char buffer[512];
    char vert_path[256];
    char frag_path[256];
    char *vert_src;
    char *frag_src;
    unsigned long ln;

    snprintf(vert_path, sizeof(vert_path), "shaders/%s.vert", name);
    snprintf(frag_path, sizeof(frag_path), "shaders/%s.frag", name);

    GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);

    if (!read_file(vert_path, &vert_src, &ln)) {
        fprintf(stderr, "failed to read vertex shader source\n");
        return 0;
    }

    if (!read_file(frag_path, &frag_src, &ln)) {
        fprintf(stderr, "failed to read fragment shader source\n");
        return 0;
    }

    glShaderSource(vert_shader, 1, (const GLchar **) &vert_src, NULL);
    glShaderSource(frag_shader, 1, (const GLchar **) &frag_src, NULL);

    free(vert_src);
    free(frag_src);

    glCompileShader(vert_shader);
    glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE) {
        glGetShaderInfoLog(vert_shader, 512, NULL, buffer);
        fprintf(stderr, "failed to compile vertex shader: %s\n", buffer);
        return 0;
    }

    glCompileShader(frag_shader);
    glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE) {
        glGetShaderInfoLog(frag_shader, 512, NULL, buffer);
        fprintf(stderr, "failed to compile fragment shader: %s\n", buffer);
        return 0;
    }

    GLuint program = glCreateProgram();

    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);

    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status != GL_TRUE) {
        glGetProgramInfoLog(program, 512, NULL, buffer);
        fprintf(stderr, "failed to link shader program: %s\n", buffer);
        return 0;
    }

    glDetachShader(program, vert_shader);
    glDetachShader(program, frag_shader);
    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    return program;
}

