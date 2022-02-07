#include "platform_layer.h"

#include <Windows.h>
#include <assert.h>
#include <Shlwapi.h>
#include <stdio.h>

aurora_platform_layer platform;

typedef struct aurora_win32_data aurora_win32_data;
struct aurora_win32_data
{
	HMODULE application_hmodule;
	HWND hwnd;
};

internal aurora_win32_data windows;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_CREATE:
		platform.quit = 0;
		break;
	case WM_CLOSE:
		platform.quit = 1;
		break;
	case WM_SIZE:
		platform.width = LOWORD(lparam);
		platform.height = HIWORD(lparam);
		if (platform.resize_event != NULL)
			platform.resize_event(platform.width, platform.height);
		break;
	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	return 0;
}

void aurora_platform_layer_init()
{
	memset(&platform, 0, sizeof(platform));
	memset(&windows, 0, sizeof(windows));
	
	windows.application_hmodule = GetModuleHandle(NULL);
	
	GetModuleFileName(windows.application_hmodule, platform.executable_directory, 512);
    PathRemoveFileSpecA(platform.executable_directory);
}

void aurora_platform_layer_free()
{
}

void aurora_platform_open_window(const char* title)
{
	WNDCLASSA wnd_class = {0};
	wnd_class.hInstance = (HINSTANCE)windows.application_hmodule;
	wnd_class.lpszClassName = "aurora_window_class";
	wnd_class.lpfnWndProc = WndProc;
	RegisterClassA(&wnd_class);
	
	windows.hwnd = CreateWindowA(wnd_class.lpszClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, platform.width, platform.height, NULL, NULL, wnd_class.hInstance, NULL);
	assert(windows.hwnd);

	ShowWindow(windows.hwnd, SW_SHOWDEFAULT);
}

void aurora_platform_update_window()
{
	MSG msg;
	while (PeekMessage(&msg, windows.hwnd, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void aurora_platform_free_window()
{
	DestroyWindow(windows.hwnd);
}

void* aurora_platform_layer_halloc(u32 size)
{
	return HeapAlloc(GetProcessHeap(), 0, size);
}

void aurora_platform_layer_hfree(void* mem)
{
	HeapFree(GetProcessHeap(), 0, mem);
}

char* aurora_platform_read_file(const char* path, u32* out_size)
{
	FILE* file = fopen(path, "rb");

	if (!file)
	{
		assert(0);
		return NULL;
	}
	else
	{
		long currentpos = ftell(file);
		fseek(file, 0, SEEK_END);
		long size = ftell(file);
		fseek(file, currentpos, SEEK_SET);

		u32 filesizepadded = (size % 4 == 0 ? size * 4 : (size + 1) * 4) / 4;

		char* buffer = malloc(filesizepadded);
		if (buffer)
		{
			fread(buffer, size, sizeof(char), file);
			fclose(file);

			*out_size = (u32)size;
			return buffer;
		}
		else
		{
			fclose(file);
			*out_size = 0;
			assert(0);
			return NULL;
		}
	}

	return NULL;
}

void aurora_platform_create_vk_surface(VkInstance instance, VkSurfaceKHR* out)
{
	VkWin32SurfaceCreateInfoKHR surface_create_info = {0};
	surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surface_create_info.hinstance = windows.application_hmodule;
	surface_create_info.hwnd = windows.hwnd;
	
	vkCreateWin32SurfaceKHR(instance, &surface_create_info, NULL, out);
}