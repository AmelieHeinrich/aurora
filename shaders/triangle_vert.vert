#version 460

layout (location = 0) out vec3 fColor;
layout (location = 1) out vec2 fTexCoords;

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vColor;
layout (location = 2) in vec2 vTexCoords;

layout (push_constant) uniform CameraConstants {
    mat4 camera;
};

void main() 
{
    fColor = vColor;
    fTexCoords = vTexCoords;
    gl_Position = camera * vec4(vPosition, 1.0);
}