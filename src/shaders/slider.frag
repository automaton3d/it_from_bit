#version 460 core

out vec4 FragColor;

uniform vec3 u_color; // Color of the object

void main()
{
    FragColor = vec4(u_color, 1.0);
}