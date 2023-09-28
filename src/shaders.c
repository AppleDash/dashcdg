#include "shaders.h"

#include "stdio.h"

GLuint load_shader_program(const char *vertexSource, const char *fragmentSource) {
    GLint status;
    char buffer[512];

    GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vert_shader, 1, &vertexSource, NULL);
    glShaderSource(frag_shader, 1, &fragmentSource, NULL);

    glCompileShader(vert_shader);
    glCompileShader(frag_shader);

    glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE) {
        glGetShaderInfoLog(vert_shader, sizeof(buffer), NULL, buffer);
        fprintf(stderr, "load_shader_program(): failed to compile vertex shader: %s\n", buffer);
        return 0;
    }

    glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE) {
        glGetShaderInfoLog(frag_shader, sizeof(buffer), NULL, buffer);
        fprintf(stderr, "load_shader_program(): failed to compile fragment shader: %s\n", buffer);
        return 0;
    }

    GLuint program = glCreateProgram();

    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);

    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status != GL_TRUE) {
        glGetProgramInfoLog(program, sizeof(buffer), NULL, buffer);
        fprintf(stderr, "failed to link shader program: %s\n", buffer);
        return 0;
    }

    glDetachShader(program, vert_shader);
    glDetachShader(program, frag_shader);

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    return program;
}
