#version 460

layout (location = 0) in vec3 fColor;

layout (location = 0) out vec4 OutColor;

void main() 
{
    OutColor = vec4(fColor, 1.0);
}