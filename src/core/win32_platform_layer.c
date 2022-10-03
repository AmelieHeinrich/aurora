#include "platform_layer.h"

#include <Windows.h>
#include <assert.h>
#include <Shlwapi.h>
#include <stdio.h>

AuroraPlatformLayer platform;

typedef struct Win32_Aurora Win32_Aurora;
struct Win32_Aurora
{
	HMODULE application_hmodule;
	HWND hwnd;
	f64 timer_start;
	f64 timer_frequency;
};

internal Win32_Aurora windows;

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

    aurora_platform_init_timer();
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

void aurora_platform_init_timer()
{
    LARGE_INTEGER large;

    QueryPerformanceCounter(&large);
    windows.timer_start = (f64)large.QuadPart;

    QueryPerformanceFrequency(&large);
    windows.timer_frequency = (f64)large.QuadPart;
}

f32 aurora_platform_get_time()
{
    LARGE_INTEGER large_int;
    QueryPerformanceCounter(&large_int);

    i64 now = large_int.QuadPart;
    i64 time = now - windows.timer_start;

    return (f32)((f64)time / windows.timer_frequency);
}

b32 aurora_platform_key_pressed(u32 key)
{
    return (b32)(GetAsyncKeyState((i32)key) & 0x8000);
}

b32 aurora_platform_mouse_button_pressed(u32 button)
{
    return (b32)(GetAsyncKeyState((i32)button) & 0x8000);
}

f32 aurora_platform_get_mouse_x()
{
    POINT p;
    GetCursorPos(&p);
    return p.x;
}

f32 aurora_platform_get_mouse_y()
{
    POINT p;
    GetCursorPos(&p);
    return p.y;
}

struct Thread
{
    HANDLE handle;
    DWORD id;
    void* ptr;
    b32 working;
    AuroraThreadWorker worker;
};

DWORD WINAPI _thread_worker(LPVOID lparam)
{
    Thread* thread = (Thread*)lparam;

    thread->working = 1;
    thread->worker(thread);

    return 0;
}

Thread* aurora_platform_new_thread(AuroraThreadWorker worker)
{
    Thread* thread = malloc(sizeof(Thread));
    thread->worker = worker;
    return thread;
}

void aurora_platform_free_thread(Thread* thread)
{
    aurora_platform_join_thread(thread);
    free(thread);
}

void aurora_platform_execute_thread(Thread* thread)
{
    thread->handle = CreateThread(NULL, 0, _thread_worker, thread, 0, &thread->id);
}

void aurora_platform_join_thread(Thread* thread)
{
    thread->working = 0;
    if (!thread->handle) return;

    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
    thread->handle = 0;
}

b32 aurora_platform_active_thread(Thread* thread)
{
    return thread->working;
}

void* aurora_platform_get_thread_ptr(Thread* thread)
{
    return thread->ptr;
}

void aurora_platform_set_thread_ptr(Thread* thread, void* ptr)
{
    thread->ptr = ptr;
}

struct Mutex
{
    HANDLE handle;
	
    void* ptr;
};

Mutex* aurora_platform_new_mutex(u64 size)
{
    Mutex* mutex = malloc(sizeof(Mutex));

    mutex->handle = CreateMutex(NULL, 0, NULL);

    if (size > 0) {
        mutex->ptr = malloc(size);
    }

    return mutex;
}

void aurora_platform_free_mutex(Mutex* mutex)
{
    CloseHandle(mutex->handle);

    if (mutex->ptr)
	free(mutex->ptr);

    free(mutex);
}

void aurora_platform_lock_mutex(Mutex* mutex)
{
    WaitForSingleObject(mutex->handle, INFINITE);
}

void aurora_platform_unlock_mutex(Mutex* mutex)
{
    ReleaseMutex(mutex->handle);
}

void* aurora_platform_mutex_get_ptr(Mutex* mutex)
{
    return mutex->ptr;
}
