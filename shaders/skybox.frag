#version 450

layout (location = 0) out vec4 OutColor;

layout (location = 0) in vec3 OutPosition;

layout (binding = 0, set = 0) uniform sampler cube_sampler;
layout (binding = 1, set = 0) uniform textureCube CubemapTexture;

void main()
{
    //OutColor = vec4(1.0f);
    OutColor = vec4(texture(samplerCube(CubemapTexture, cube_sampler), OutPosition).rgb, 1.0);
}