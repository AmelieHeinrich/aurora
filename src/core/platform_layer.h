/* date = February 2nd 2022 6:44 pm */

#ifndef PLATFORM_LAYER_H
#define PLATFORM_LAYER_H

#include "common.h"

#include <volk.h>

typedef void (*AuroraResizeEvent)(u32, u32);

typedef struct AuroraPlatformLayer AuroraPlatformLayer;
struct AuroraPlatformLayer
{	
	u32 width;
	u32 height;
	b32 quit;
	
	char executable_directory[512];

	AuroraResizeEvent resize_event;
};

extern AuroraPlatformLayer platform;

void  aurora_platform_layer_init();
void  aurora_platform_layer_free();
char* aurora_platform_read_file(const char* path, u32* out_size);
void  aurora_platform_open_window(const char* title);
void  aurora_platform_update_window();
void  aurora_platform_free_window();
void  aurora_platform_create_vk_surface(VkInstance instance, VkSurfaceKHR* out);
void  aurora_platform_init_timer();
f32   aurora_platform_get_time();
b32   aurora_platform_key_pressed(u32 key);
b32   aurora_platform_mouse_button_pressed(u32 button);
f32   aurora_platform_get_mouse_x();
f32   aurora_platform_get_mouse_y();

#endif //PLATFORM_LAYER_H
