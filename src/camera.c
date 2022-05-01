#include "camera.h"
#include "platform_layer.h"

#include <stdlib.h>
#include <string.h>

#define HMM_Vec3s(scalar) HMM_Vec3(scalar, scalar, scalar)

hmm_vec4 vec4_from_plane(hmm_vec3 norm, hmm_vec3 point)
{
    hmm_vec4 tmp;
    tmp.XYZ = norm;
    tmp.W = HMM_DotVec3(norm, point);
    return tmp;
}

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
    camera->width = (f32)platform.width;
    camera->height = (f32)platform.height;

    f32 mouse_x = aurora_platform_get_mouse_x();
    f32 mouse_y = aurora_platform_get_mouse_y();

    camera->mouse_pos.X = mouse_x;
    camera->mouse_pos.Y = mouse_y;

    camera->view = HMM_LookAt(camera->position, HMM_AddVec3(camera->position, camera->front), camera->worldup);
    camera->projection = HMM_Perspective(75.0f, camera->width / camera->height, 0.001f, 10000.0f);
    camera->view_projection = HMM_MultiplyMat4(camera->projection, camera->view);
}

void fps_camera_update_frustum(fps_camera* camera)
{
    const f32 half_v_side = 10000.0f * tanf(HMM_ToRadians(75.0f) * .5f);
    const f32 half_h_side = half_v_side * (camera->width / camera->height);
    const hmm_vec3 front_mult_far = HMM_MultiplyVec3f(camera->front, 10000.0f);

    camera->view_frustum.near.point = HMM_AddVec3(camera->position, HMM_MultiplyVec3f(camera->front, 0.001f));
    camera->view_frustum.near.norm = camera->front;

    camera->view_frustum.far.point = HMM_AddVec3(camera->position, front_mult_far);
    camera->view_frustum.far.norm = HMM_MultiplyVec3f(camera->front, -1.0f);

    camera->view_frustum.right.point = camera->position;
    camera->view_frustum.right.norm = HMM_Cross(camera->up, HMM_AddVec3(front_mult_far, HMM_MultiplyVec3f(camera->right, half_h_side)));

    camera->view_frustum.left.point = camera->position;
    camera->view_frustum.left.norm = HMM_Cross(HMM_SubtractVec3(front_mult_far, HMM_MultiplyVec3f(camera->right, half_h_side)), camera->up);

    camera->view_frustum.top.point = camera->position;
    camera->view_frustum.top.norm = HMM_Cross(camera->right, HMM_SubtractVec3(front_mult_far, HMM_MultiplyVec3f(camera->up, half_v_side)));

    camera->view_frustum.bottom.point = camera->position;
    camera->view_frustum.bottom.norm = HMM_Cross(HMM_AddVec3(front_mult_far, HMM_MultiplyVec3f(camera->up, half_v_side)), camera->right);

    // //

    camera->frustum_planes[0].XYZ = HMM_NormalizeVec3(camera->view_frustum.near.norm);
    camera->frustum_planes[0].W = HMM_DotVec3(camera->frustum_planes[0].XYZ, camera->view_frustum.near.point);

    camera->frustum_planes[1].XYZ = HMM_NormalizeVec3(camera->view_frustum.far.norm);
    camera->frustum_planes[1].W = HMM_DotVec3(camera->frustum_planes[1].XYZ, camera->view_frustum.far.point);

    camera->frustum_planes[2].XYZ = HMM_NormalizeVec3(camera->view_frustum.right.norm);
    camera->frustum_planes[2].W = HMM_DotVec3(camera->frustum_planes[2].XYZ, camera->view_frustum.right.point);

    camera->frustum_planes[3].XYZ = HMM_NormalizeVec3(camera->view_frustum.left.norm);
    camera->frustum_planes[3].W = HMM_DotVec3(camera->frustum_planes[3].XYZ, camera->view_frustum.left.point);

    camera->frustum_planes[4].XYZ = HMM_NormalizeVec3(camera->view_frustum.top.norm);
    camera->frustum_planes[4].W = HMM_DotVec3(camera->frustum_planes[4].XYZ, camera->view_frustum.top.point);

    camera->frustum_planes[5].XYZ = HMM_NormalizeVec3(camera->view_frustum.bottom.norm);
    camera->frustum_planes[5].W = HMM_DotVec3(camera->frustum_planes[5].XYZ, camera->view_frustum.bottom.point);
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