#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec2 vTexcoords;
layout (location = 2) in vec3 vNormals;

layout (location = 0) out vec3 fPosition;
layout (location = 1) out vec2 fTexcoords;
layout (location = 2) out vec3 fNormals;

layout (push_constant) uniform CameraConstants {
    mat4 camera;
};

layout (binding = 0, set = 2) uniform BindlessMaterial {
    mat4 ModelMatrix;
    uint TextureIndex;
    uint SamplerIndex;
};

void main() 
{
    fPosition = vec3(camera * ModelMatrix * vec4(vPosition, 1.0));
    fTexcoords = vTexcoords;
    fNormals = vNormals;
    gl_Position = camera * ModelMatrix * vec4(vPosition, 1.0);
}