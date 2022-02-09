#version 460

layout (location = 0) out vec4 OutColor;

layout (location = 0) in vec3 fColor;
layout (location = 1) in vec2 fTexCoords;

layout (binding = 0, set = 0) uniform texture2D TextureHeap[64];
layout (binding = 0, set = 1) uniform sampler   SamplerHeap[8];
layout (binding = 0, set = 2) uniform BindlessMaterial {
    uint TextureIndex;
    uint SamplerIndex;
};

void main() 
{
    vec4 color = texture(sampler2D(TextureHeap[TextureIndex], SamplerHeap[SamplerIndex]), fTexCoords);

    OutColor = vec4(color.rgb, 1.0);
}