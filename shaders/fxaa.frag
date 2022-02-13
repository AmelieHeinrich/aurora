#version 450

#define FXAA_THRESHOLD 1.0 / 4.0
#define MAX_SPAN 8.0
#define MUL_REDUCE 1.0 / MAX_SPAN
#define MIN_REDUCE 1.0 / 128.0

layout (location = 0) in vec3 OutPosition;
layout (location = 1) in vec2 OutUV;

layout (location = 0) out vec4 OutColor;

layout (binding = 0, set = 0) uniform texture2D color_image;
layout (binding = 0, set = 1) uniform sampler   SamplerHeap[8];

layout (push_constant) uniform FXAASettings {
	vec2 screen_size;
    vec2 pad;
} settings;

vec3 apply_fxaa(vec4 uv, texture2D tex, sampler samp, vec2 rcpFrame)
{
	vec3 rgbNW = textureLod(sampler2D(tex, samp), uv.zw, 0.0).xyz;
    vec3 rgbNE = textureLod(sampler2D(tex, samp), uv.zw + vec2(1,0) * rcpFrame.xy, 0.0).xyz;
    vec3 rgbSW = textureLod(sampler2D(tex, samp), uv.zw + vec2(0,1) * rcpFrame.xy, 0.0).xyz;
    vec3 rgbSE = textureLod(sampler2D(tex, samp), uv.zw + vec2(1,1) * rcpFrame.xy, 0.0).xyz;
    vec3 rgbM  = textureLod(sampler2D(tex, samp), uv.xy, 0.0).xyz;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * MUL_REDUCE),
        MIN_REDUCE);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    
    dir = min(vec2( MAX_SPAN,  MAX_SPAN),
          max(vec2(-MAX_SPAN, -MAX_SPAN),
          dir * rcpDirMin)) * rcpFrame.xy;

    vec3 rgbA = (1.0/2.0) * (
        textureLod(sampler2D(tex, samp), uv.xy + dir * (1.0/3.0 - 0.5), 0.0).xyz +
        textureLod(sampler2D(tex, samp), uv.xy + dir * (2.0/3.0 - 0.5), 0.0).xyz);
    vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
        textureLod(sampler2D(tex, samp), uv.xy + dir * (0.0/3.0 - 0.5), 0.0).xyz +
        textureLod(sampler2D(tex, samp), uv.xy + dir * (3.0/3.0 - 0.5), 0.0).xyz);
    
    float lumaB = dot(rgbB, luma);

    if((lumaB < lumaMin) || (lumaB > lumaMax)) return rgbA;
    
    return rgbB;
}

void main()
{
    vec2 rcpFrame = 1.0 / settings.screen_size;
  	vec2 uv2 = OutUV;

    vec4 uv = vec4( uv2, uv2 - (rcpFrame * (0.5 + FXAA_THRESHOLD)));
	vec3 col = apply_fxaa(uv, color_image, SamplerHeap[0], 1.0 / settings.screen_size.xy);
    
    const mat3 inputMatrix = mat3
    (
		vec3(0.59719, 0.07600, 0.02840),
		vec3(0.35458, 0.90834, 0.13383),
		vec3(0.04823, 0.01566, 0.83777)
    );
    
    const mat3 outputMatrix = mat3
    (
		vec3(1.60475, -0.10208, -0.00327),
		vec3(-0.53108, 1.10813, -0.07276),
		vec3(-0.07367, -0.00605, 1.07602)
    );
    
    vec3 inputColour = inputMatrix * col;
    vec3 a = inputColour * (inputColour + vec3(0.0245786)) - vec3(0.000090537);
    vec3 b = inputColour * (0.983729 * inputColour + 0.4329510) + 0.238081;
    vec3 c = a / b;
    col = pow(max(outputMatrix * c, 0.0.xxx), vec3(1. / 2.2));

    OutColor = vec4(col, 1.0);
}