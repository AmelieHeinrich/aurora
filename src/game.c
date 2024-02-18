#include "game.h"

#include <core/platform_layer.h>
#include <core/random.h>
#include <client/camera.h>
#include <gfx/rhi.h>
#include <gfx/render_graph.h>
#include <gfx/geometry_pass.h>
#include <gfx/fxaa_pass.h>
#include <gfx/final_blit_pass.h>
#include <audio/audio.h>
#include <resource/mesh.h>

#include <stdio.h>
#include <time.h>

typedef struct GameData GameData;
struct GameData
{
    FPS_Camera camera;
    f64 last_frame;
    b32 update_frustum;

    RenderGraph rg;
    RenderGraphExecute rge;
    RenderGraphNode* gp;
    RenderGraphNode* fxaap;
    RenderGraphNode* fbp;

    Mesh test_model;

    Thread* audio_thread;
    AudioClip debug_music;
};

internal GameData data;

void game_resize(u32 width, u32 height)
{
    rhi_wait_idle();
    data.rge.width = width;
    data.rge.height = height;
    rhi_resize();
    fps_camera_resize(&data.camera, width, height);
    resize_render_graph(&data.rg, &data.rge);
}

void game_init()
{
    data.update_frustum = 1;

    srand(time(NULL));

    aurora_platform_layer_init();
    platform.width = 1280;
    platform.height = 720;
    platform.resize_event = game_resize;
    aurora_platform_open_window("Aurora Window");

    rhi_init();
    fps_camera_init(&data.camera);
    init_render_graph(&data.rg, &data.rge);

    for (i32 i = 0; i < TEST_LIGHT_COUNT; i++)
    {
     data.rge.light_info.lights[i].position.X = random_float(-3.0f, 3.0f);
     data.rge.light_info.lights[i].position.Y = random_float(-1.0f, -5.0f);
     data.rge.light_info.lights[i].position.Z = random_float(-3.0f, 3.0f);

     data.rge.light_info.lights[i].color.X = random_float(0.1f, 4.0f);
     data.rge.light_info.lights[i].color.Y = random_float(0.1f, 4.0f);
     data.rge.light_info.lights[i].color.Z = random_float(0.1f, 4.0f);
    }
    data.rge.light_info.light_count = TEST_LIGHT_COUNT;

    f64 start = aurora_platform_get_time();

#if TEST_MODEL_SPONZA
    mesh_load(&data.test_model, "assets/Sponza.gltf");
#elif TEST_MODEL_HELMET
    mesh_load(&data.test_model, "assets/DamagedHelmet.gltf");
#endif
    f64 end = aurora_platform_get_time();
    printf("Model loaded in %f seconds", end - start);

    data.rge.models[0] = data.test_model;
    data.rge.model_count++;

    data.gp = create_geometry_pass();
    data.fxaap = create_fxaa_pass();
    data.fbp = create_final_blit_pass();
     
    connect_render_graph_nodes(&data.rg, GeometryPassOutputLit, FXAAPassInputColor, data.gp, data.fxaap);
    connect_render_graph_nodes(&data.rg, FXAAPassOutputAntiAliased, FinalBlitPassInputImage, data.fxaap, data.fbp);
    bake_render_graph(&data.rg, &data.rge, data.fbp);
}

void game_update()
{
    while (!platform.quit)
    {
        f32 time = aurora_platform_get_time();
    f32 dt = time - data.last_frame;
    data.last_frame = time;

    data.rge.camera.projection = data.camera.projection;
    data.rge.camera.view = data.camera.view;
        data.rge.camera.pos = data.camera.position;

    if (aurora_platform_key_pressed(KEY_W))
        data.update_frustum = 0;

    rhi_begin();
    update_render_graph(&data.rg, &data.rge);
    rhi_end();

    f32 start = aurora_platform_get_time();
    rhi_present();
    f32 end = aurora_platform_get_time();

    fps_camera_input(&data.camera, dt);
    fps_camera_update(&data.camera, dt);
    if (data.update_frustum)
        fps_camera_update_frustum(&data.camera);

    for (i32 i = 0; i < 6; i++)
        data.rge.camera.frustrum_planes[i] = data.camera.frustum_planes[i];

        aurora_platform_update_window();
   }
}

void game_exit()
{
    aurora_platform_join_thread(data.audio_thread);
    aurora_platform_free_thread(data.audio_thread);

    rhi_wait_idle();

    mesh_free(&data.test_model);

    free_render_graph(&data.rg, &data.rge);

    mesh_loader_free();
    rhi_free_descriptor_heap(&data.rge.image_heap);
    rhi_free_descriptor_heap(&data.rge.sampler_heap);
    rhi_shutdown();
    
    aurora_platform_free_window();
    aurora_platform_layer_free();
}
