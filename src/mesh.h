#ifndef MESH_H_INCLUDED
#define MESH_H_INCLUDED

#include "common.h"
#include "rhi.h"
#include "HandmadeMath.h"

#define MAX_PRIMITIVES 128
#define MAX_MESHLET_VERTICES 64
#define MAX_MESHLET_INDICES 372
#define MAX_MESHLET_TRIANGLES 124

typedef struct vertex vertex;
struct vertex
{
    hmm_vec3 position;
    hmm_vec2 uv;
    hmm_vec3 normals;
};

typedef struct meshlet meshlet;
struct meshlet
{
    u32 vertices[MAX_MESHLET_VERTICES];
    u8 indices[MAX_MESHLET_INDICES];
    u8 vertex_count;
    u8 triangle_count;
};

typedef struct gltf_material gltf_material;
struct gltf_material
{
    rhi_image albedo;
    i32 albedo_bindless_index;
    rhi_image normal;
    i32 normal_bindless_index;
    rhi_image metallic_roughness;
    i32 metallic_roughness_index;

    hmm_vec3 base_color_factor;
    f32 metallic_factor;
    f32 roughness_factor;

    rhi_buffer material_buffer;
    rhi_descriptor_set material_set;
};

typedef struct primitive primitive;
struct primitive
{
    rhi_buffer vertex_buffer;
    rhi_buffer index_buffer;
    rhi_buffer meshlet_buffer;
    rhi_descriptor_set geometry_descriptor_set;

    u32 vertex_size;
    u32 index_size;

    u32 vertex_count;
    u32 index_count;
    u32 triangle_count;
    u32 meshlet_count;
    u32 material_index;
};

typedef struct mesh mesh;
struct mesh
{
    primitive primitives[MAX_PRIMITIVES];
    i32 primitive_count;

    gltf_material materials[MAX_PRIMITIVES];
    i32 material_count;

    u32 total_vertex_count;
    u32 total_index_count;
    u32 total_triangle_count;
    u32 total_meshlet_count;

    char* directory;
};

void mesh_loader_init(i32 dset_layout_binding);
void mesh_loader_free();
rhi_descriptor_set_layout* mesh_loader_get_descriptor_set_layout();
rhi_descriptor_set_layout* mesh_loader_get_geometry_descriptor_set_layout();
void mesh_loader_set_texture_heap(rhi_descriptor_heap* heap);
void mesh_load(mesh* out, const char* path);
void mesh_free(mesh* m);

#endif