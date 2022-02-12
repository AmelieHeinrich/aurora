#include "mesh.h"
#include "cgltf.h"

#include <assert.h>
#include <stdlib.h>

#define cgltf_call(call) do { cgltf_result _result = (call); assert(_result == cgltf_result_success); } while(0)

u32 cgltf_comp_size(cgltf_component_type type)
{
    switch (type)
    {
    case cgltf_component_type_r_8:
    case cgltf_component_type_r_8u:
        return 1;
    case cgltf_component_type_r_16:
    case cgltf_component_type_r_16u:
        return 2;
    case cgltf_component_type_r_32u:
    case cgltf_component_type_r_32f:
        return 4;
    }

    assert(0);
    return 0;
}

u32 cgltf_comp_count(cgltf_type type)
{
    switch (type)
    {
    case cgltf_type_scalar:
        return 1;
    case cgltf_type_vec2:
        return 2;
    case cgltf_type_vec3:
        return 3;
    case cgltf_type_vec4:
    case cgltf_type_mat2:
        return 4;
    case cgltf_type_mat3:
        return 9;
    case cgltf_type_mat4:
        return 16;
    }

    assert(0);
    return 0;
}

void* cgltf_get_accessor_data(cgltf_accessor* accessor, u32* component_size, u32* component_count)
{
    *component_size = cgltf_comp_size(accessor->component_type);
    *component_count = cgltf_comp_count(accessor->type);

    cgltf_buffer_view* view = accessor->buffer_view;
    return OFFSET_PTR_BYTES(void, view->buffer->data, view->offset);
}

void cgltf_process_primitive(cgltf_primitive* primitive, u32 primitive_index, submesh* sm, mesh* m)
{
    if (primitive->type != cgltf_primitive_type_triangles)
        return;

    cgltf_attribute* position_attribute = 0;
    cgltf_attribute* texcoord_attribute = 0;
    cgltf_attribute* normal_attribute = 0;

    for (i32 attribute_index = 0; attribute_index < primitive->attributes_count; attribute_index++)
    {
        cgltf_attribute* attribute = &primitive->attributes[attribute_index];

        if (strcmp(attribute->name, "POSITION") == 0) position_attribute = attribute;
        if (strcmp(attribute->name, "TEXCOORD_0") == 0) texcoord_attribute = attribute;
        if (strcmp(attribute->name, "NORMAL") == 0) normal_attribute = attribute;
    }

    assert(position_attribute && texcoord_attribute && normal_attribute);

    u32 vertex_count = (u32)normal_attribute->data->count;
    u64 vertices_size = vertex_count * sizeof(vertex);
    vertex* vertices = (vertex*)malloc(vertices_size);
    memset(vertices, 0, sizeof(vertices));

    {
        u32 component_size, component_count;
        f32* src = (f32*)cgltf_get_accessor_data(position_attribute->data, &component_size, &component_count);
        assert(component_size == 4);

        if (src)
        {
            for (u32 vertex_index = 0; vertex_index < vertex_count; vertex_index++)
            {
                vertices[vertex_index].position.X = src[vertex_index * component_count + 0];
                vertices[vertex_index].position.Y = src[vertex_index * component_count + 1];
                vertices[vertex_index].position.Z = src[vertex_index * component_count + 2];
            }
        }
    }

    {
        u32 component_size, component_count;
        f32* src = (f32*)cgltf_get_accessor_data(texcoord_attribute->data, &component_size, &component_count);
        assert(component_size == 4);

        if (src)
        {
            for (u32 vertex_index = 0; vertex_index < vertex_count; vertex_index++)
            {
                vertices[vertex_index].uv.X = src[vertex_index * component_count + 0];
                vertices[vertex_index].uv.Y = src[vertex_index * component_count + 1];
            }
        }
    }

    {
        u32 component_size, component_count;
        f32* src = (f32*)cgltf_get_accessor_data(normal_attribute->data, &component_size, &component_count);
        assert(component_size == 4);

        if (src)
        {
            for (u32 vertex_index = 0; vertex_index < vertex_count; vertex_index++)
            {
                vertices[vertex_index].normals.X = src[vertex_index * component_count + 0];
                vertices[vertex_index].normals.Y = src[vertex_index * component_count + 1];
                vertices[vertex_index].normals.Z = src[vertex_index * component_count + 2];
            }
        }
    }

    u32 index_count = (u32)primitive->indices->count;
    u32* indices = (u32*)malloc(index_count * sizeof(u32));
    memset(indices, 0, sizeof(indices));

    {
        u32 component_size, component_count;
        void* src = cgltf_get_accessor_data(primitive->indices, &component_size, &component_count);
        assert(component_size == 4 || component_size == 2);

        if (component_size == 2)
        { // u16
            u16* ptr = (u16*)src;
            for (u32 index_index = 0; index_index < index_count; index_index++)
                indices[index_index] = ptr[index_index];
        }
        else
        { // u32
            u32* ptr = (u32*)src;
            for (u32 index_index = 0; index_index < index_count; index_index++)
                indices[index_index] = ptr[index_index];
        }
    }

    rhi_allocate_buffer(&sm->vertex_buffer, vertices_size, BUFFER_VERTEX);
    rhi_upload_buffer(&sm->vertex_buffer, vertices, vertices_size);

    rhi_allocate_buffer(&sm->index_buffer, index_count * sizeof(u32), BUFFER_INDEX);
    rhi_upload_buffer(&sm->index_buffer, indices, index_count * sizeof(u32));

    // TODO: Meshlets

    sm->vertex_count = vertex_count;
    sm->index_count = index_count;
    sm->triangle_count = index_count / 3;
    sm->vertex_size = vertices_size;
    sm->index_size = index_count * sizeof(u32);

    m->total_vertex_count += sm->vertex_count;
    m->total_index_count += sm->index_count;
    m->total_triangle_count += sm->triangle_count;

    free(indices);
    free(vertices);

    primitive_index++;
}

void cgltf_process_node(cgltf_node* node, u32 primitive_index, mesh* m)
{
    if (node->mesh)
    {
        for (i32 p = 0; p < node->mesh->primitives_count; p++)
        {
            cgltf_process_primitive(&node->mesh->primitives[p], primitive_index, &m->submeshes[p], m);
            m->submesh_count++;
        }
    }

    for (i32 c = 0; c < node->children_count; c++)
        cgltf_process_node(node->children[c], primitive_index, m);
}

void mesh_load(mesh* out, const char* path)
{
    memset(out, 0, sizeof(mesh));
    memset(out->submeshes, 0, sizeof(out->submeshes));

    cgltf_options options;
    memset(&options, 0, sizeof(options));
    cgltf_data* data = 0;

    cgltf_call(cgltf_parse_file(&options, path, &data));
    cgltf_call(cgltf_load_buffers(&options, data, path));
    cgltf_scene* scene = data->scene;
    
    u32 pi = 0;
    for (i32 ni = 0; ni < scene->nodes_count; ni++)
        cgltf_process_node(scene->nodes[ni], pi, out);

    cgltf_free(data);
}

void mesh_free(mesh* m)
{
    for (i32 i = 0; i < m->submesh_count; i++)
    {
        rhi_free_buffer(&m->submeshes[i].index_buffer);
        rhi_free_buffer(&m->submeshes[i].vertex_buffer);
    }
}