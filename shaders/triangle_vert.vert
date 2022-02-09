#version 460

layout (location = 0) out vec3 fColor;
layout (location = 1) out vec2 fTexCoords;

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vColor;
layout (location = 2) in vec2 vTexCoords;

void main() 
{
    fColor = vColor;
    fTexCoords = vTexCoords;
    gl_Position = vec4(vPosition, 1.0);
}