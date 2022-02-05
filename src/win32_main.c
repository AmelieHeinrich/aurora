#include "platform_layer.h"
#include "rhi.h"

#include <stdio.h>

int main()
{
	aurora_platform_layer_init();
	platform.width = 1280;
	platform.height = 720;
	aurora_platform_open_window("Aurora Window");

	rhi_init();

	while (!platform.quit)
	{
		aurora_platform_update_window();
	}

	rhi_shutdown();
	
	aurora_platform_free_window();
	aurora_platform_layer_free();
	return 0;
}
