// !!! This isn't the actual shader used, you need to change shaders.h to reflect changes to this!
#version 130
#extension GL_EXT_gpu_shader4 : enable

uniform int[16] cdgColorMap;
uniform sampler2D cdgFramebuffer;

// Coordinate of the vertex in the framebuffer
in vec2 vertexCoord;

void main() {
    ivec2 index = ivec2(vertexCoord.x, vertexCoord.y);
    int colorIndex = int(texelFetch(cdgFramebuffer, index, 0).r * 255);

    int rgb = cdgColorMap[colorIndex];

    gl_FragColor = vec4(
        float((rgb >> 16) & 0xFF) / 255.0,
        float((rgb >> 8) & 0xFF) / 255.0,
        float((rgb & 0xFF)) / 255.0,
        1.0
    );
}
