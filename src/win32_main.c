#include "platform_layer.h"
#include "rhi.h"
#include "camera.h"
#include "mesh.h"
#include "render_graph.h"
#include "geometry_pass.h"
#include "final_blit_pass.h"

#include <stdio.h>

global fps_camera camera;
global f64 last_frame;
global render_graph rg;
global render_graph_execute rge;
global render_graph_node* gp;
global render_graph_node* fbp;
global render_graph_drawable sponza;

void game_resize(u32 width, u32 height)
{
	rhi_wait_idle();
	rge.width = width;
	rge.height = height;
	rhi_resize();
	fps_camera_resize(&camera, width, height);
	resize_render_graph(&rg, &rge);
}

int main()
{
	aurora_platform_layer_init();
	platform.width = 1280;
	platform.height = 720;
	platform.resize_event = game_resize;
	aurora_platform_open_window("Aurora Window");

	rhi_init();
	fps_camera_init(&camera);
	init_render_graph(&rg, &rge);

	mesh_load(&sponza.m, "assets/Sponza.gltf");
	sponza.model_matrix = HMM_Mat4d(1.0f);
	sponza.model_matrix = HMM_Scale(HMM_Vec3(0.00800000037997961, 0.00800000037997961, 0.00800000037997961));
	sponza.model_matrix = HMM_MultiplyMat4(sponza.model_matrix, HMM_Rotate(180.0f, HMM_Vec3(1.0f, 0.0f, 0.0f)));
	rge.drawables[0] = sponza;
	rge.drawable_count++;

	gp = create_geometry_pass();
	fbp = create_final_blit_pass();

	connect_render_graph_nodes(&rg, geometry_pass_output_lit, final_blit_pass_input_image, gp, fbp);
	bake_render_graph(&rg, &rge, fbp);

	while (!platform.quit)
	{
		f32 time = aurora_platform_get_time();
		f32 dt = time - last_frame;
		last_frame = time;

		rge.camera.vp = camera.view_projection;
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

	mesh_free(&sponza.m);

	free_render_graph(&rg, &rge);

	mesh_loader_free();
	rhi_free_descriptor_heap(&rge.image_heap);
	rhi_free_descriptor_heap(&rge.sampler_heap);
	rhi_shutdown();
	
	aurora_platform_free_window();
	aurora_platform_layer_free();
	return 0;
}
