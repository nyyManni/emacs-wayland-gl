#version 320 es

layout (location = 0) in vec2 vertex;

uniform mat4 projection;
uniform vec2 offset;

out VS_OUT {
    int glyph;
} vs_out;

void main() {
    gl_Position = vec4(vertex.xy + offset, 0.0, 1.0);
    vs_out.glyph = 36;
}
