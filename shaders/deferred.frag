#version 460

#define PI 3.14159265359
#define MAX_LIGHTS 512

struct PointLight
{
    vec3 position;
    float pad;
    vec3 color;
    float pad2;
};

layout (location = 0) out vec4 OutColor;

layout (location = 0) in vec2 fTexcoords;

layout (binding = 0, set = 0) uniform texture2D gPosition;
layout (binding = 1, set = 0) uniform texture2D gNormal;
layout (binding = 2, set = 0) uniform texture2D gAlbedo;
layout (binding = 3, set = 0) uniform texture2D gMetallicRoughness;
layout (binding = 0, set = 1) uniform sampler   SamplerHeap[16];

layout (binding = 0, set = 2) uniform Lights {
    PointLight lights[MAX_LIGHTS];
    int light_count;
    vec3 _light_pad;
};

layout (push_constant) uniform CameraConstants {
    vec3 fCameraPos;
    float pad;
};

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
    vec3 FragPos = texture(sampler2D(gPosition, SamplerHeap[0]), fTexcoords.xy).rgb;
    vec3 N = texture(sampler2D(gNormal, SamplerHeap[0]), fTexcoords.xy).rgb;
    vec4 Diffuse = texture(sampler2D(gAlbedo, SamplerHeap[0]), fTexcoords.xy);
    vec4 MR = texture(sampler2D(gMetallicRoughness, SamplerHeap[0]), fTexcoords.xy);

    float metallic = MR.g;
    float roughness = MR.b;

    if (metallic == 0.0)
        metallic = 0.1;
    if (roughness == 0.0)
        roughness = 0.5;

    if (Diffuse.a < 0.25)
        discard;
    Diffuse.rgb = pow(Diffuse.rgb, vec3(2.2));

    float ao = 1.0f;

    vec3 V = normalize(fCameraPos - FragPos);
    vec3 R = reflect(-V, N); 

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, Diffuse.rgb, metallic);

    vec3 Lo = vec3(0.0);

    for (int i = 0; i < light_count; i++)
    {
        vec3 L = normalize(lights[i].position - FragPos);
        vec3 H = normalize(V + L);
        float distance = length(lights[i].position - FragPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = lights[i].color * attenuation;

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

        Lo += (kD * Diffuse.rgb / PI + specular) * radiance * NdotL;    
    }      

    vec3 ambient = vec3(0.03) * Diffuse.rgb * ao;
    vec3 color = ambient + Lo;

    OutColor = vec4(color, 1.0);
}