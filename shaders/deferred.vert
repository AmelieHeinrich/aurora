#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec2 vTexCoords;

layout (location = 0) out vec2 fTexCoords;

void main()
{
    fTexCoords = vTexCoords;
    gl_Position = vec4(vPosition, 1.0);
}