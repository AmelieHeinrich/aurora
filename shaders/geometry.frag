#version 460

#define PI 3.14159265359

layout (location = 0) out vec4 OutColor;

layout (location = 0) in vec3 fPosition;
layout (location = 1) in vec2 fTexcoords;
layout (location = 2) in vec3 fNormals;
layout (location = 3) in vec3 fCameraPos;

layout (binding = 0, set = 1) uniform texture2D TextureHeap[512];
layout (binding = 0, set = 2) uniform sampler   SamplerHeap[512];
layout (binding = 0, set = 3) uniform BindlessMaterial {
    uint AlbedoIndex;
    uint NormalIndex;
    uint MetallicRoughnessIndex;
    vec3 BaseColorFactor;
    float MetallicFactor;
    float RoughnessFactor;
};

vec3 GetNormalFromMap()
{
    vec3 tangentNormal = texture(sampler2D(TextureHeap[NormalIndex], SamplerHeap[0]), fTexcoords).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(fPosition);
    vec3 Q2  = dFdy(fPosition);
    vec2 st1 = dFdx(fTexcoords);
    vec2 st2 = dFdy(fTexcoords);

    vec3 N   = normalize(fNormals);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001); // prevent divide by zero for roughness=0.0 and NdotH=1.0
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}   

void main() 
{   
    vec4 albedo = texture(sampler2D(TextureHeap[AlbedoIndex], SamplerHeap[0]), fTexcoords.xy) * vec4(BaseColorFactor, 1.0);
    if (albedo.a < 0.25)
        discard;

    float ao = 1.0f;
    float metallic = texture(sampler2D(TextureHeap[MetallicRoughnessIndex], SamplerHeap[0]), fTexcoords.xy).g * MetallicFactor;
    float roughness = texture(sampler2D(TextureHeap[MetallicRoughnessIndex], SamplerHeap[0]), fTexcoords.xy).b * RoughnessFactor;
    vec3 N = GetNormalFromMap();

    if (metallic == 0.0)
        metallic = MetallicFactor;
    if (roughness == 0.0)
        roughness = RoughnessFactor;
    if (N.r == 0 || N.g == 0 || N.b == 0)
        N = normalize(fNormals);

    vec3 V = normalize(fCameraPos - fPosition);
    vec3 R = reflect(-V, N); 

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo.rgb, metallic);

    vec3 Lo = vec3(0.0);

    {
        vec3 L = normalize(fCameraPos - fPosition);
        vec3 H = normalize(V + L);
        float distance = length(fCameraPos - fPosition);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = vec3(20.0) * vec3(attenuation);

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);    
        vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);        

        vec3 numerator    = NDF * G * F;
        float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	      

        float NdotL = max(dot(N, L), 0.0);        

        Lo += (kD * albedo.rgb / PI + specular) * radiance * NdotL;    
    }      

    vec3 ambient = vec3(0.03) * albedo.rgb * ao;
    vec3 color = ambient + Lo;

    OutColor = albedo;
}