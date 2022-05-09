#version 450

layout (location = 0) in PerVertexData {
    vec3 fPosition;
    vec2 fTexcoords;
    vec3 fNormals;
    vec3 fCameraPos;
    vec3 fMeshletColor;
} FragmentIn;

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedo;
layout (location = 3) out vec4 gMetallicRoughness;

layout (binding = 0, set = 1) uniform texture2D TextureHeap[512];
layout (binding = 0, set = 2) uniform sampler   SamplerHeap[512];
layout (binding = 0, set = 3) uniform BindlessMaterial {
    uvec4 BindlessIndex; // x = albedo, y = normal, z = mr, w = albedo sampler
    vec3 color_factor;
    float metallic_factor;
    float roughness_factor;
    vec3 pad0;
};

layout (binding = 0, set = 5) uniform RenderParams {
    bool show_meshlets;
    vec3 pad;
} params;

vec3 GetNormalFromMap()
{
    vec3 tangentNormal = texture(sampler2D(TextureHeap[BindlessIndex.y], SamplerHeap[0]), FragmentIn.fTexcoords).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(FragmentIn.fPosition);
    vec3 Q2  = dFdy(FragmentIn.fPosition);
    vec2 st1 = dFdx(FragmentIn.fTexcoords);
    vec2 st2 = dFdy(FragmentIn.fTexcoords);

    vec3 N   = normalize(FragmentIn.fNormals);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main()
{    
    vec3 N = GetNormalFromMap();
    vec4 alb = texture(sampler2D(TextureHeap[BindlessIndex.x], SamplerHeap[BindlessIndex.w]), FragmentIn.fTexcoords) * vec4(color_factor, 1.0);
    vec4 mr = texture(sampler2D(TextureHeap[BindlessIndex.z], SamplerHeap[0]), FragmentIn.fTexcoords);

    if (metallic_factor > 0)
        mr.b *= metallic_factor;
    if (roughness_factor > 0)
        mr.g *= roughness_factor;

    if (alb.a < 0.25)
        discard;

    gPosition = FragmentIn.fPosition;
    gNormal = N;
    gAlbedo = params.show_meshlets ? vec4(FragmentIn.fMeshletColor, 1.0) : alb;
    gMetallicRoughness = mr;
}