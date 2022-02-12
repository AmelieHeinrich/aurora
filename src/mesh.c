#include "mesh.h"
#include "cgltf.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define cgltf_call(call) do { cgltf_result _result = (call); assert(_result == cgltf_result_success); } while(0)

internal rhi_descriptor_heap* s_image_heap;
internal rhi_descriptor_set_layout s_descriptor_set_layout;

void mesh_loader_init(i32 dset_layout_binding)
{
    s_descriptor_set_layout.binding = dset_layout_binding;
    s_descriptor_set_layout.descriptors[0] = DESCRIPTOR_BUFFER;
    s_descriptor_set_layout.descriptor_count = 1;
    
    rhi_init_descriptor_set_layout(&s_descriptor_set_layout);
}

void mesh_loader_free()
{
    rhi_free_descriptor_set_layout(&s_descriptor_set_layout);
}

rhi_descriptor_set_layout* mesh_loader_get_descriptor_set_layout()
{
    return &s_descriptor_set_layout;
}

void mesh_loader_set_texture_heap(rhi_descriptor_heap* heap)
{
    s_image_heap = heap;
}

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

void cgltf_process_primitive(cgltf_primitive* cgltf_primitive, u32* primitive_index, mesh* m)
{
    primitive* pri = &m->primitives[(*primitive_index)++];

    if (cgltf_primitive->type != cgltf_primitive_type_triangles)
        return;

    cgltf_attribute* position_attribute = 0;
    cgltf_attribute* texcoord_attribute = 0;
    cgltf_attribute* normal_attribute = 0;

    for (i32 attribute_index = 0; attribute_index < cgltf_primitive->attributes_count; attribute_index++)
    {
        cgltf_attribute* attribute = &cgltf_primitive->attributes[attribute_index];

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

    pri->index_count = (u32)cgltf_primitive->indices->count;
    u32 index_size = pri->index_count * sizeof(u32);
    u32* indices = (u32*)malloc(index_size);
    memset(indices, 0, index_size);

    {
        if (cgltf_primitive->indices != NULL)
        {
            for (u32 k = 0; k < (u32)cgltf_primitive->indices->count; k++)
                indices[k] = (u32)(cgltf_accessor_read_index(cgltf_primitive->indices, k));
        }
    }

    rhi_allocate_buffer(&pri->vertex_buffer, vertices_size, BUFFER_VERTEX);
    rhi_upload_buffer(&pri->vertex_buffer, vertices, vertices_size);

    rhi_allocate_buffer(&pri->index_buffer, index_size, BUFFER_INDEX);
    rhi_upload_buffer(&pri->index_buffer, indices, index_size);

    // TODO: Meshlets

    typedef struct temp_mat
    {
        i32 albedo_idx;
        i32 normal_idx;
        i32 mr_idx;
        hmm_vec3 bc_factor;
        f32 m_factor;
        f32 r_factor;
    } temp_mat;

    // Load textures
    {
        if (cgltf_primitive->material)
        {
            pri->material_index = m->material_count;

            {
                char tx_path[512];
                sprintf(tx_path, "%s%s", m->directory, cgltf_primitive->material->pbr_metallic_roughness.base_color_texture.texture->image->uri);
                printf("%s\n", tx_path);

                rhi_load_image(&m->materials[pri->material_index].albedo, tx_path);
                m->materials[pri->material_index].albedo_bindless_index = rhi_find_available_descriptor(s_image_heap);
                rhi_push_descriptor_heap_image(s_image_heap, &m->materials[pri->material_index].albedo, m->materials[pri->material_index].albedo_bindless_index);
            
                m->materials[pri->material_index].base_color_factor.X = cgltf_primitive->material->pbr_metallic_roughness.base_color_factor[0];
                m->materials[pri->material_index].base_color_factor.Y = cgltf_primitive->material->pbr_metallic_roughness.base_color_factor[1];
                m->materials[pri->material_index].base_color_factor.Z = cgltf_primitive->material->pbr_metallic_roughness.base_color_factor[2];
            }

            {
                if (cgltf_primitive->material->normal_texture.texture)
                {
                    char tx_path[512];
                    sprintf(tx_path, "%s%s", m->directory, cgltf_primitive->material->normal_texture.texture->image->uri);
                    printf("%s\n", tx_path);

                    rhi_load_image(&m->materials[pri->material_index].normal, tx_path);
                    m->materials[pri->material_index].normal_bindless_index = rhi_find_available_descriptor(s_image_heap);
                    rhi_push_descriptor_heap_image(s_image_heap, &m->materials[pri->material_index].normal, m->materials[pri->material_index].normal_bindless_index);
                }
            }

            {
                if (cgltf_primitive->material->pbr_metallic_roughness.metallic_roughness_texture.texture)
                {
                    char tx_path[512];
                    sprintf(tx_path, "%s%s", m->directory, cgltf_primitive->material->pbr_metallic_roughness.metallic_roughness_texture.texture->image->uri);
                    printf("%s\n", tx_path);

                    rhi_load_image(&m->materials[pri->material_index].metallic_roughness, tx_path);
                    m->materials[pri->material_index].metallic_roughness_index = rhi_find_available_descriptor(s_image_heap);
                    rhi_push_descriptor_heap_image(s_image_heap, &m->materials[pri->material_index].metallic_roughness, m->materials[pri->material_index].metallic_roughness_index);
                
                    m->materials[pri->material_index].metallic_factor = cgltf_primitive->material->pbr_metallic_roughness.metallic_factor;
                    m->materials[pri->material_index].roughness_factor = cgltf_primitive->material->pbr_metallic_roughness.roughness_factor;
                }
            }
        
            temp_mat temp;
            temp.albedo_idx = m->materials[pri->material_index].albedo_bindless_index;
            temp.normal_idx = m->materials[pri->material_index].normal_bindless_index;
            temp.mr_idx = m->materials[pri->material_index].metallic_roughness_index;
            temp.bc_factor = m->materials[pri->material_index].base_color_factor;
            temp.m_factor = m->materials[pri->material_index].metallic_factor;
            temp.r_factor = m->materials[pri->material_index].roughness_factor;

            rhi_allocate_buffer(&m->materials[pri->material_index].material_buffer, sizeof(temp_mat), BUFFER_UNIFORM);
            rhi_upload_buffer(&m->materials[pri->material_index].material_buffer, &temp, sizeof(temp_mat));

            rhi_init_descriptor_set(&m->materials[pri->material_index].material_set, &s_descriptor_set_layout);
            rhi_descriptor_set_write_buffer(&m->materials[pri->material_index].material_set, &m->materials[pri->material_index].material_buffer, sizeof(temp_mat), 0);

            m->material_count++;
        }
    }

    pri->vertex_count = vertex_count;
    pri->triangle_count = pri->index_count / 3;
    pri->vertex_size = vertices_size;
    pri->index_size = index_size;

    m->total_vertex_count += pri->vertex_count;
    m->total_index_count += pri->index_count;
    m->total_triangle_count += pri->triangle_count;

    free(indices);
    free(vertices);
}

void cgltf_process_node(cgltf_node* node, u32* primitive_index, mesh* m)
{
    if (node->mesh)
    {
        for (i32 p = 0; p < node->mesh->primitives_count; p++)
        {
            cgltf_process_primitive(&node->mesh->primitives[p], primitive_index, m);
            m->primitive_count++;
        }
    }

    for (i32 c = 0; c < node->children_count; c++)
        cgltf_process_node(node->children[c], primitive_index, m);
}

void mesh_load(mesh* out, const char* path)
{
    memset(out, 0, sizeof(mesh));

    cgltf_options options;
    memset(&options, 0, sizeof(options));
    cgltf_data* data = 0;

    cgltf_call(cgltf_parse_file(&options, path, &data));
    cgltf_call(cgltf_load_buffers(&options, data, path));
    cgltf_scene* scene = data->scene;
    
    const char* ch = "/";
    char* ptr = strstr(path, ch);
    ptr += sizeof(char);
    strncpy(ptr, "", strlen(ptr));
    out->directory = (char*)path;

    u32 pi = 0;
    for (i32 ni = 0; ni < scene->nodes_count; ni++)
        cgltf_process_node(scene->nodes[ni], &pi, out);

    cgltf_free(data);
}

void mesh_free(mesh* m)
{
    for (i32 i = 0; i < m->primitive_count; i++)
    {
        rhi_free_buffer(&m->primitives[i].index_buffer);
        rhi_free_buffer(&m->primitives[i].vertex_buffer);
    }

    for (i32 i = 0; i < m->material_count; i++)
    {
        if (m->materials[i].albedo.image != VK_NULL_HANDLE)
            rhi_free_image(&m->materials[i].albedo);
        if (m->materials[i].normal.image != VK_NULL_HANDLE)
            rhi_free_image(&m->materials[i].normal);
        if (m->materials[i].metallic_roughness.image != VK_NULL_HANDLE)
            rhi_free_image(&m->materials[i].metallic_roughness);
        rhi_free_buffer(&m->materials[i].material_buffer);
        rhi_free_descriptor_set(&m->materials[i].material_set);
    }
}