#ifndef MESH_H_INCLUDED
#define MESH_H_INCLUDED

#include <core/common.h>
#include <gfx/rhi.h>

#include <HandmadeMath.h>

#define MAX_PRIMITIVES 128
#define MAX_MESHLET_VERTICES 64
#define MAX_MESHLET_INDICES 372
#define MAX_MESHLET_TRIANGLES 124

typedef struct Vertex Vertex;
struct Vertex
{
    hmm_vec3 position;
    hmm_vec2 uv;
    hmm_vec3 normals;
};

#pragma pack(push, 16)
typedef struct Meshlet Meshlet;
struct Meshlet
{
    hmm_vec4 sphere;

    u32 vertices[MAX_MESHLET_VERTICES];
    u8 indices[MAX_MESHLET_INDICES];
    u8 vertex_count;
    u8 triangle_count;
};
#pragma pack(pop)

typedef struct GLTFMaterial GLTFMaterial;
struct GLTFMaterial
{
    RHI_Image albedo;
    i32 albedo_bindless_index;
    RHI_Sampler albedo_sampler;
    i32 albedo_sampler_index;

    RHI_Image normal;
    i32 normal_bindless_index;

    RHI_Image metallic_roughness;
    i32 metallic_roughness_index;

    hmm_vec3 base_color_factor;
    f32 metallic_factor;
    f32 roughness_factor;

    RHI_Buffer material_buffer;
    RHI_DescriptorSet material_set;
};

typedef struct Primitive Primitive;
struct Primitive
{
    RHI_Buffer vertex_buffer;
    RHI_Buffer index_buffer;
    RHI_Buffer meshlet_buffer;
    RHI_DescriptorSet geometry_descriptor_set;

    u32 vertex_size;
    u32 index_size;

    u32 vertex_count;
    u32 index_count;
    u32 triangle_count;
    u32 meshlet_count;
    u32 material_index;
};

typedef struct Mesh Mesh;
struct Mesh
{
    Primitive primitives[MAX_PRIMITIVES];
    i32 primitive_count;

    GLTFMaterial materials[MAX_PRIMITIVES];
    i32 material_count;

    u32 total_vertex_count;
    u32 total_index_count;
    u32 total_triangle_count;
    u32 total_meshlet_count;

    char* directory;
};

void mesh_loader_init(i32 dset_layout_binding);
void mesh_loader_free();
RHI_DescriptorSetLayout* mesh_loader_get_descriptor_set_layout();
RHI_DescriptorSetLayout* mesh_loader_get_geometry_descriptor_set_layout();
void mesh_loader_set_texture_heap(RHI_DescriptorHeap* heap);
void mesh_loader_set_sampler_heap(RHI_DescriptorHeap* heap);
void mesh_load(Mesh* out, const char* path);
void mesh_free(Mesh* m);

#endif