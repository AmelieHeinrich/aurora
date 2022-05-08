#include "game.h"

#include <core/platform_layer.h>
#include <core/random_gen.h>
#include <client/camera.h>
#include <gfx/rhi.h>
#include <gfx/render_graph.h>
#include <gfx/geometry_pass.h>
#include <gfx/fxaa_pass.h>
#include <gfx/final_blit_pass.h>
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

    RenderGraphDrawable test_model;
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
		data.rge.light_info.lights[i].position.X = float_rand(-3.0f, 3.0f);
		data.rge.light_info.lights[i].position.Y = float_rand(-1.0f, -5.0f);
		data.rge.light_info.lights[i].position.Z = float_rand(-3.0f, 3.0f);

		data.rge.light_info.lights[i].color.X = float_rand(0.1f, 4.0f);
		data.rge.light_info.lights[i].color.Y = float_rand(0.1f, 4.0f);
		data.rge.light_info.lights[i].color.Z = float_rand(0.1f, 4.0f);
	}
	data.rge.light_info.light_count = TEST_LIGHT_COUNT;

#if TEST_MODEL_SPONZA
	mesh_load(&data.test_model.m, "assets/Sponza.gltf");
	data.test_model.model_matrix = HMM_Mat4d(1.0f);
	data.test_model.model_matrix = HMM_Scale(HMM_Vec3(0.00800000037997961, 0.00800000037997961, 0.00800000037997961));
	data.test_model.model_matrix = HMM_MultiplyMat4(data.test_model.model_matrix, HMM_Rotate(-180.0f, HMM_Vec3(1.0f, 0.0f, 0.0f)));
#elif TEST_MODEL_HELMET
	mesh_load(&data.test_model.m, "assets/DamagedHelmet.gltf");
	data.test_model.model_matrix = HMM_Mat4d(1.0f);
	data.test_model.model_matrix = HMM_MultiplyMat4(data.test_model.model_matrix, HMM_Rotate(180.0f, HMM_Vec3(0.0f, 1.0f, 0.0f)));
	data.test_model.model_matrix = HMM_MultiplyMat4(data.test_model.model_matrix, HMM_Rotate(-90.0f, HMM_Vec3(1.0f, 0.0f, 0.0f)));
#endif

	data.rge.drawables[0] = data.test_model;
	data.rge.drawable_count++;

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

		rhi_present();

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
    rhi_wait_idle();

	mesh_free(&data.test_model.m);

	free_render_graph(&data.rg, &data.rge);

	mesh_loader_free();
	rhi_free_descriptor_heap(&data.rge.image_heap);
	rhi_free_descriptor_heap(&data.rge.sampler_heap);
	rhi_shutdown();
	
	aurora_platform_free_window();
	aurora_platform_layer_free();
}