#include "mesh.h"
#include "cgltf.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define cgltf_call(call) do { cgltf_result _result = (call); assert(_result == cgltf_result_success); } while(0)

internal rhi_descriptor_heap* s_image_heap;
internal rhi_descriptor_set_layout s_descriptor_set_layout;
internal rhi_descriptor_set_layout s_meshlet_set_layout;

typedef struct aabb aabb;
struct aabb
{
    hmm_vec3 min;
    hmm_vec3 max;
    hmm_vec3 extents;
    hmm_vec3 center;
};

typedef struct meshlet_vector meshlet_vector;
struct meshlet_vector
{
    meshlet* meshlets;
    u32 used;
    u32 size;
};

void init_meshlet_vector(meshlet_vector* vec, u32 start_size)
{
    vec->meshlets = calloc(start_size, sizeof(meshlet));
    vec->size = start_size;
    vec->used = 0;
}

void free_meshlet_vector(meshlet_vector* vec)
{
    free(vec->meshlets);
}

void push_meshlet(meshlet_vector* vec, meshlet m)
{
    if (vec->used >= vec->size)
    {
        vec->size *= 2;
        vec->meshlets = realloc(vec->meshlets, vec->size * sizeof(meshlet));
    }
    vec->meshlets[vec->used++] = m;
}

void mesh_loader_init(i32 dset_layout_binding)
{
    s_descriptor_set_layout.binding = dset_layout_binding;
    s_descriptor_set_layout.descriptors[0] = DESCRIPTOR_BUFFER;
    s_descriptor_set_layout.descriptor_count = 1;
    rhi_init_descriptor_set_layout(&s_descriptor_set_layout);

    s_meshlet_set_layout.binding = 0;
    s_meshlet_set_layout.descriptors[0] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    s_meshlet_set_layout.descriptors[1] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    s_meshlet_set_layout.descriptor_count = 2;
    rhi_init_descriptor_set_layout(&s_meshlet_set_layout);
}

void mesh_loader_free()
{
    rhi_free_descriptor_set_layout(&s_meshlet_set_layout);
    rhi_free_descriptor_set_layout(&s_descriptor_set_layout);
}

rhi_descriptor_set_layout* mesh_loader_get_descriptor_set_layout()
{
    return &s_descriptor_set_layout;
}

rhi_descriptor_set_layout* mesh_loader_get_geometry_descriptor_set_layout()
{
    return &s_meshlet_set_layout;
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

    meshlet_vector vec;
    init_meshlet_vector(&vec, 256);

    u8* meshlet_vertices = (u8*)malloc(sizeof(u8) * vertex_count);
    memset(meshlet_vertices, 0xff, sizeof(u8) * vertex_count);

    meshlet ml;
    memset(&ml, 0, sizeof(ml));

    for (i64 i = 0; i < pri->index_count; i += 3)
    {
        u32 a = indices[i + 0];
        u32 b = indices[i + 1];
        u32 c = indices[i + 2];

        u8 av = meshlet_vertices[a];
        u8 bv = meshlet_vertices[b];
        u8 cv = meshlet_vertices[c];

        u32 used_extra = (av == 0xff) + (bv == 0xff) + (cv == 0xff);

        if (ml.vertex_count + used_extra > MAX_MESHLET_VERTICES || ml.triangle_count >= MAX_MESHLET_TRIANGLES)
        {
            push_meshlet(&vec, ml);
            
            for (size_t j = 0; j < ml.vertex_count; ++j)
                meshlet_vertices[ml.vertices[j]] = 0xff;

            memset(&ml, 0, sizeof(ml));
        }

        if (av == 0xff)
        {
            av = ml.vertex_count;
            ml.vertices[ml.vertex_count++] = a;
        }

        if (bv == 0xff)
        {
            bv = ml.vertex_count;
            ml.vertices[ml.vertex_count++] = b;
        }

        if (cv == 0xff)
        {
            cv = ml.vertex_count;
            ml.vertices[ml.vertex_count++] = c;
        }

        ml.indices[ml.triangle_count * 3 + 0] = av;
        ml.indices[ml.triangle_count * 3 + 1] = bv;
        ml.indices[ml.triangle_count * 3 + 2] = cv;
        ml.triangle_count++;
    }

    if (ml.triangle_count)
        push_meshlet(&vec, ml);

    for (u32 i = 0; i < vec.used; i++)
    {
        hmm_vec3 mean_normal;
        memset(&mean_normal, 0, sizeof(hmm_vec3));

        aabb bbox;
        memset(&bbox, 0, sizeof(aabb));
        f32 radius = 0.0f;
        bbox.min = vertices[vec.meshlets[i].vertices[vec.meshlets[i].indices[0]]].position;
        bbox.max = bbox.min;

        for (u32 j = 0; j < vec.meshlets[i].vertex_count; ++j)
        {
            u32 a = vec.meshlets[i].indices[j];
            const vertex* va = &vertices[vec.meshlets[i].vertices[a]];

            mean_normal = HMM_AddVec3(mean_normal, va->normals);

            // Compute AABB

            bbox.min.X = min(bbox.min.X, va->position.X);
            bbox.min.Y = min(bbox.min.Y, va->position.Y);
            bbox.min.Z = min(bbox.min.Z, va->position.Z);

            bbox.max.X = max(bbox.max.X, va->position.X);
            bbox.max.Y = max(bbox.max.Y, va->position.Y);
            bbox.max.Z = max(bbox.max.Z, va->position.Z);
        }
        bbox.extents = HMM_SubtractVec3(bbox.max, bbox.min);
        bbox.center = HMM_AddVec3(bbox.min, bbox.extents);
        mean_normal = HMM_NormalizeVec3(mean_normal);

        f32 angular_span = 0.0f;

        for (u32 j = 0; j < vec.meshlets[i].vertex_count; ++j)
        {
            u32 a = vec.meshlets[i].indices[j];
            const vertex* va = &vertices[vec.meshlets[i].vertices[a]];

            angular_span = max(angular_span, acos(HMM_DotVec3(mean_normal, va->normals)));

            f32 distance = sqrt(pow((bbox.center.X - va->position.X), 2) + pow((bbox.center.Y - va->position.Y), 2) + pow((bbox.center.Z - va->position.Z), 2));
            radius = max(radius, distance);
        }

        vec.meshlets[i].cone.X = mean_normal.X;
        vec.meshlets[i].cone.Y = mean_normal.Y;
        vec.meshlets[i].cone.Z = mean_normal.Z;
        vec.meshlets[i].cone.W = angular_span;

        vec.meshlets[i].sphere.X = bbox.center.X;
        vec.meshlets[i].sphere.Y = bbox.center.Y;
        vec.meshlets[i].sphere.Z = bbox.center.Z;
        vec.meshlets[i].sphere.W = radius;
    }

    rhi_allocate_buffer(&pri->meshlet_buffer, vec.used * sizeof(meshlet), BUFFER_VERTEX);
    rhi_upload_buffer(&pri->meshlet_buffer, vec.meshlets, vec.used * sizeof(meshlet));

    rhi_init_descriptor_set(&pri->geometry_descriptor_set, &s_meshlet_set_layout);
    rhi_descriptor_set_write_storage_buffer(&pri->geometry_descriptor_set, &pri->vertex_buffer, vertices_size, 0);
    rhi_descriptor_set_write_storage_buffer(&pri->geometry_descriptor_set, &pri->meshlet_buffer, vec.used * sizeof(meshlet), 1);

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
    pri->meshlet_count = vec.used;

    m->total_vertex_count += pri->vertex_count;
    m->total_index_count += pri->index_count;
    m->total_triangle_count += pri->triangle_count;

    free_meshlet_vector(&vec);
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
        rhi_free_buffer(&m->primitives[i].meshlet_buffer);
        rhi_free_buffer(&m->primitives[i].index_buffer);
        rhi_free_buffer(&m->primitives[i].vertex_buffer);
        rhi_free_descriptor_set(&m->primitives[i].geometry_descriptor_set);
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