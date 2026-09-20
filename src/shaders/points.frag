#version 460 core
in vec4 fragColor;
out vec4 color;

void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    if (length(coord) > 0.5)
        discard; // Make points circular
    color = fragColor;
}
