#version 460

layout (location = 0) out vec4 OutColor;

layout (location = 0) in vec3 fPosition;
layout (location = 1) in vec2 fTexcoords;
layout (location = 2) in vec3 fNormals;

layout (binding = 0, set = 0) uniform texture2D TextureHeap[512];
layout (binding = 0, set = 1) uniform sampler   SamplerHeap[512];
layout (binding = 0, set = 2) uniform BindlessMaterial {
    mat4 ModelMatrix;
    uint TextureIndex;
    uint SamplerIndex;
};

void main() 
{   
    vec4 color = texture(sampler2D(TextureHeap[TextureIndex], SamplerHeap[SamplerIndex]), fTexcoords.xy);

    OutColor = vec4(fNormals, 1.0);
}