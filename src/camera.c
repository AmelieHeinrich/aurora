#include "camera.h"
#include "platform_layer.h"

#include <stdlib.h>
#include <string.h>

#define HMM_Vec3s(scalar) HMM_Vec3(scalar, scalar, scalar)

void fps_camera_update_vectors(fps_camera* camera)
{
    hmm_vec3 front;
    front.X = cos(HMM_ToRadians(camera->yaw)) * cos(HMM_ToRadians(camera->pitch));
    front.Y = sin(HMM_ToRadians(camera->pitch));
    front.Z = sin(HMM_ToRadians(camera->yaw)) * cos(HMM_ToRadians(camera->pitch));
    camera->front = HMM_NormalizeVec3(front);

    camera->right = HMM_NormalizeVec3(HMM_Cross(camera->front, camera->worldup));
    camera->up = HMM_NormalizeVec3(HMM_Cross(camera->right, camera->front));
}

void fps_camera_init(fps_camera* camera)
{   
    memset(camera, 0, sizeof(fps_camera));

    camera->up.Y = -1.0f;
    camera->front.Z = -1.0f;
    camera->worldup.Y = 1.0f;
    camera->position.Z = 1.0f;
    camera->yaw = CAMERA_DEFAULT_YAW;
    camera->pitch = CAMERA_DEFAULT_PITCH;
    camera->zoom = CAMERA_DEFAULT_ZOOM;
    camera->friction = 10.0f;
    camera->acceleration = 20.0f;
    camera->max_velocity = 15.0f;
    camera->width = (f32)platform.width;
    camera->height = (f32)platform.height;

    fps_camera_update_vectors(camera);
}

void fps_camera_update(fps_camera* camera, f32 dt)
{
    f32 mouse_x = aurora_platform_get_mouse_x();
    f32 mouse_y = aurora_platform_get_mouse_y();

    camera->mouse_pos.X = mouse_x;
    camera->mouse_pos.Y = mouse_y;

    camera->view = HMM_LookAt(camera->position, HMM_AddVec3(camera->position, camera->front), camera->worldup);
    camera->projection = HMM_Perspective(75.0f, camera->width / camera->height, 0.001f, 10000.0f);
    camera->view_projection = HMM_MultiplyMat4(camera->projection, camera->view);
}

void fps_camera_input(fps_camera* camera, f32 dt)
{
    f32 speed_multiplier = camera->acceleration * dt;
    if (aurora_platform_key_pressed(KEY_Z))
        camera->velocity = HMM_AddVec3(camera->velocity, HMM_MultiplyVec3f(camera->front, speed_multiplier));
    if (aurora_platform_key_pressed(KEY_S))
        camera->velocity = HMM_SubtractVec3(camera->velocity, HMM_MultiplyVec3f(camera->front, speed_multiplier));
    if (aurora_platform_key_pressed(KEY_Q))
        camera->velocity = HMM_SubtractVec3(camera->velocity, HMM_MultiplyVec3f(camera->right, speed_multiplier));
    if (aurora_platform_key_pressed(KEY_D))
        camera->velocity = HMM_AddVec3(camera->velocity, HMM_MultiplyVec3f(camera->right, speed_multiplier));

    f32 friction_multiplier = 1.0f / (1.0f + (camera->friction * dt));
    camera->velocity = HMM_MultiplyVec3f(camera->velocity, friction_multiplier);

    f32 vec_length = HMM_LengthVec3(camera->velocity);
    if (vec_length > camera->max_velocity)
        camera->velocity = HMM_MultiplyVec3f(HMM_NormalizeVec3(camera->velocity), camera->max_velocity);

    camera->position = HMM_AddVec3(camera->position, HMM_MultiplyVec3f(camera->velocity, dt));

    f32 mouse_x = aurora_platform_get_mouse_x();
    f32 mouse_y = aurora_platform_get_mouse_y();

    f32 dx = (mouse_x - camera->mouse_pos.X) * (CAMERA_DEFAULT_MOUSE_SENSITIVITY * dt);
    f32 dy = (mouse_y - camera->mouse_pos.Y) * (CAMERA_DEFAULT_MOUSE_SENSITIVITY * dt);

    if (aurora_platform_mouse_button_pressed(MOUSE_LEFT))
    {
        camera->yaw += dx;
        camera->pitch += dy;
    }

    fps_camera_update_vectors(camera);
}

void fps_camera_resize(fps_camera* camera, i32 width, i32 height)
{
    camera->width = width;
    camera->height = height;

    fps_camera_update_vectors(camera);
}   