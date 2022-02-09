#version 460

layout (location = 0) out vec4 OutColor;

layout (location = 0) in vec3 fColor;
layout (location = 1) in vec2 fTexCoords;

void main() 
{
    OutColor = vec4(fColor, 1.0);
}