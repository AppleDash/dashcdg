#version 130

out vec2 vertexCoord;

void main() {
    vertexCoord = gl_Vertex.xy;

    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}