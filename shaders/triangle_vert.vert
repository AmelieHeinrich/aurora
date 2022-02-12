#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec2 vTexcoords;
layout (location = 2) in vec3 vNormals;

layout (location = 0) out vec3 fPosition;
layout (location = 1) out vec2 fTexcoords;
layout (location = 2) out vec3 fNormals;
layout (location = 3) out vec3 fCameraPos;

layout (push_constant) uniform CameraConstants {
    mat4 ModelMatrix;
    mat4 camera;
    vec3 camera_pos;
    float pad;
};

void main() 
{
    fPosition = vec3(ModelMatrix * vec4(vPosition, 1.0));
    fTexcoords = vTexcoords;
    fNormals = transpose(inverse(mat3(ModelMatrix))) * vNormals;
    fCameraPos = camera_pos;
    gl_Position = camera * ModelMatrix * vec4(vPosition, 1.0);
}