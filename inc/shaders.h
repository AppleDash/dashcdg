#ifndef _SHADERS_H_INCLUDED
#define _SHADERS_H_INCLUDED

#include <GL/glew.h>

#define CDG_VERTEX_SHADER_SOURCE "#version 130\n \
out vec2 vertexCoord; \
void main() { \
    vertexCoord = gl_Vertex.xy; \
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex; \
}"

#define CDG_FRAGMENT_SHADER_SOURCE "#version 130\n \
#extension GL_EXT_gpu_shader4 : enable\n \
uniform int[16] cdgColorMap; \
uniform sampler2D cdgFramebuffer; \
in vec2 vertexCoord; \
void main() { \
    ivec2 index = ivec2(vertexCoord.x, vertexCoord.y); \
    int colorIndex = int(texelFetch(cdgFramebuffer, index, 0).r * 255); \
    int rgb = cdgColorMap[colorIndex]; \
    gl_FragColor = vec4( \
        float((rgb >> 16) & 0xFF) / 255.0, \
        float((rgb >> 8) & 0xFF) / 255.0, \
        float((rgb & 0xFF)) / 255.0, \
        1.0 \
    ); \
}"

GLuint load_shader_program(const char *vertexSource, const char *fragmentSource);

#endif
