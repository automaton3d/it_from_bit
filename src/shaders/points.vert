#version 460 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;

out vec4 fragColor;

void main() {
    gl_Position = vec4(position, 1.0);
    gl_PointSize = 10.0; // Set point size here
    fragColor = color;
}
