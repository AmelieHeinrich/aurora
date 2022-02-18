#include "platform_layer.h"
#include "rhi.h"
#include "camera.h"
#include "mesh.h"
#include "render_graph.h"
#include "geometry_pass.h"
#include "fxaa_pass.h"
#include "final_blit_pass.h"

#include <stdio.h>
#include <time.h>

#define TEST_LIGHT_COUNT 64
#define TEST_MODEL_SPONZA 0
#define TEST_MODEL_HELMET 1

global fps_camera camera;
global f64 last_frame;

global render_graph rg;
global render_graph_execute rge;
global render_graph_node* gp;
global render_graph_node* fxaap;
global render_graph_node* fbp;
global render_graph_drawable test_model;

void game_resize(u32 width, u32 height)
{
	rhi_wait_idle();
	rge.width = width;
	rge.height = height;
	rhi_resize();
	fps_camera_resize(&camera, width, height);
	resize_render_graph(&rg, &rge);
}

f32 float_rand(f32 min, f32 max)
{
    f32 scale = rand() / (f32)RAND_MAX; /* [0, 1.0] */
    return min + scale * ( max - min );      /* [min, max] */
}

int main()
{
	srand(time(NULL));

	aurora_platform_layer_init();
	platform.width = 1280;
	platform.height = 720;
	platform.resize_event = game_resize;
	aurora_platform_open_window("Aurora Window");

	rhi_init();
	fps_camera_init(&camera);
	init_render_graph(&rg, &rge);

	for (i32 i = 0; i < TEST_LIGHT_COUNT; i++)
	{
		rge.light_info.lights[i].position.X = float_rand(-3.0f, 3.0f);
		rge.light_info.lights[i].position.Y = float_rand(-1.0f, -5.0f);
		rge.light_info.lights[i].position.Z = float_rand(-3.0f, 3.0f);

		rge.light_info.lights[i].color.X = float_rand(0.1f, 4.0f);
		rge.light_info.lights[i].color.Y = float_rand(0.1f, 4.0f);
		rge.light_info.lights[i].color.Z = float_rand(0.1f, 4.0f);
	}
	rge.light_info.light_count = TEST_LIGHT_COUNT;

#if TEST_MODEL_SPONZA
	mesh_load(&test_model.m, "assets/Sponza.gltf");
	test_model.model_matrix = HMM_Mat4d(1.0f);
	test_model.model_matrix = HMM_Scale(HMM_Vec3(0.00800000037997961, 0.00800000037997961, 0.00800000037997961));
	test_model.model_matrix = HMM_MultiplyMat4(test_model.model_matrix, HMM_Rotate(-180.0f, HMM_Vec3(1.0f, 0.0f, 0.0f)));
#elif TEST_MODEL_HELMET
	mesh_load(&test_model.m, "assets/DamagedHelmet.gltf");
	test_model.model_matrix = HMM_Mat4d(1.0f);
	test_model.model_matrix = HMM_MultiplyMat4(test_model.model_matrix, HMM_Rotate(180.0f, HMM_Vec3(0.0f, 1.0f, 0.0f)));
	test_model.model_matrix = HMM_MultiplyMat4(test_model.model_matrix, HMM_Rotate(-90.0f, HMM_Vec3(1.0f, 0.0f, 0.0f)));
#endif

	rge.drawables[0] = test_model;
	rge.drawable_count++;

	gp = create_geometry_pass();
	fxaap = create_fxaa_pass();
	fbp = create_final_blit_pass();

	connect_render_graph_nodes(&rg, geometry_pass_output_lit, fxaa_pass_input_color, gp, fxaap);
	connect_render_graph_nodes(&rg, fxaa_pass_output_anti_aliased, final_blit_pass_input_image, fxaap, fbp);
	bake_render_graph(&rg, &rge, fbp);

	while (!platform.quit)
	{
		f32 time = aurora_platform_get_time();
		f32 dt = time - last_frame;
		last_frame = time;

		rge.camera.projection = camera.projection;
		rge.camera.view = camera.view;
		rge.camera.pos = camera.position;

		rhi_begin();
		update_render_graph(&rg, &rge);
		rhi_end();

		rhi_present();

		fps_camera_input(&camera, dt);
		fps_camera_update(&camera, dt);

		aurora_platform_update_window();
	}
	
	rhi_wait_idle();

	mesh_free(&test_model.m);

	free_render_graph(&rg, &rge);

	mesh_loader_free();
	rhi_free_descriptor_heap(&rge.image_heap);
	rhi_free_descriptor_heap(&rge.sampler_heap);
	rhi_shutdown();
	
	aurora_platform_free_window();
	aurora_platform_layer_free();
	return 0;
}
