#version 460

layout (location = 0) out vec3 fColor;

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vColor;

void main() 
{
    fColor = vColor;
    gl_Position = vec4(vPosition, 1.0);
}