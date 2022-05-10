#version 450

layout (location = 0) out vec3 OutPosition;

layout (push_constant) uniform SceneUniform {
    mat4 vp;
} scene;

vec3 skyboxVertices[] = vec3[](          
        vec3(-100.0f,  100.0f, -100.0f),
        vec3(-100.0f, -100.0f, -100.0f),
        vec3( 100.0f, -100.0f, -100.0f),
        vec3( 100.0f, -100.0f, -100.0f),
        vec3( 100.0f,  100.0f, -100.0f),
        vec3(-100.0f,  100.0f, -100.0f),
        vec3(-100.0f, -100.0f,  100.0f),
        vec3(-100.0f, -100.0f, -100.0f),
        vec3(-100.0f,  100.0f, -100.0f),
        vec3(-100.0f,  100.0f, -100.0f),
        vec3(-100.0f,  100.0f,  100.0f),
        vec3(-100.0f, -100.0f,  100.0f),
        vec3( 100.0f, -100.0f, -100.0f),
        vec3( 100.0f, -100.0f,  100.0f),
        vec3( 100.0f,  100.0f,  100.0f),
        vec3( 100.0f,  100.0f,  100.0f),
        vec3( 100.0f,  100.0f, -100.0f),
        vec3( 100.0f, -100.0f, -100.0f),
        vec3(-100.0f, -100.0f,  100.0f),
        vec3(-100.0f,  100.0f,  100.0f),
        vec3( 100.0f,  100.0f,  100.0f),
        vec3( 100.0f,  100.0f,  100.0f),
        vec3( 100.0f, -100.0f,  100.0f),
        vec3(-100.0f, -100.0f,  100.0f),
        vec3(-100.0f,  100.0f, -100.0f),
        vec3( 100.0f,  100.0f, -100.0f),
        vec3( 100.0f,  100.0f,  100.0f),
        vec3( 100.0f,  100.0f,  100.0f),
        vec3(-100.0f,  100.0f,  100.0f),
        vec3(-100.0f,  100.0f, -100.0f),
        vec3(-100.0f, -100.0f, -100.0f),
        vec3(-100.0f, -100.0f,  100.0f),
        vec3( 100.0f, -100.0f, -100.0f),
        vec3( 100.0f, -100.0f, -100.0f),
        vec3(-100.0f, -100.0f,  100.0f),
        vec3( 100.0f, -100.0f,  100.0f)
);

void main()
{
    vec4 pos = scene.vp * vec4(skyboxVertices[gl_VertexIndex], 1.0);
    pos.z = pos.w;
    gl_Position = pos;
    OutPosition = skyboxVertices[gl_VertexIndex];
}