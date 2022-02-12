#ifndef MESH_H_INCLUDED
#define MESH_H_INCLUDED

#include "common.h"
#include "rhi.h"
#include "HandmadeMath.h"

#define MAX_SUBMESHES 128
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
    u32 indices[MAX_MESHLET_INDICES];
    u32 vertex_count;
    u32 triangle_count;
};

typedef struct submesh submesh;
struct submesh
{
    rhi_buffer vertex_buffer;
    rhi_buffer index_buffer;
    rhi_buffer meshlet_buffer;

    u32 vertex_size;
    u32 index_size;

    u32 vertex_count;
    u32 index_count;
    u32 triangle_count;
    u32 meshlet_count;
};

typedef struct mesh mesh;
struct mesh
{
    submesh submeshes[MAX_SUBMESHES];
    i32 submesh_count;

    u32 total_vertex_count;
    u32 total_index_count;
    u32 total_triangle_count;
    u32 total_meshlet_count;
};

void mesh_load(mesh* out, const char* path);
void mesh_free(mesh* m);

#endif