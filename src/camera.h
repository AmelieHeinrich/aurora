#ifndef CAMERA_H_INCLUDED
#define CAMERA_H_INCLUDED

#include "common.h"
#include "HandmadeMath.h"

#undef near
#undef far

#define CAMERA_DEFAULT_YAW -90.0f
#define CAMERA_DEFAULT_PITCH 0.0f
#define CAMERA_DEFAULT_SPEED 1.0f
#define CAMERA_DEFAULT_MOUSE_SENSITIVITY 5.0f
#define CAMERA_DEFAULT_ZOOM 90.0f

typedef struct plane plane;
struct plane
{
    hmm_vec3 point;
    hmm_vec3 norm;
};

typedef struct frustum frustum;
struct frustum
{
    plane near;
    plane far;
    plane right;
    plane left;
    plane top;
    plane bottom;
};

typedef struct fps_camera fps_camera;
struct fps_camera
{
    f32 yaw;
    f32 pitch;
    f32 zoom;

    hmm_vec3 position;
    hmm_vec3 front;
    hmm_vec3 up;
    hmm_vec3 right;
    hmm_vec3 worldup;

    hmm_vec2 mouse_pos;
    b32 first_mouse;

    hmm_mat4 view;
    hmm_mat4 projection;
    hmm_mat4 view_projection;

    f32 width;
    f32 height;

    f32 acceleration;
    f32 friction;
    hmm_vec3 velocity;
    f32 max_velocity;

    frustum view_frustum;
    hmm_vec4 frustum_planes[6];
};

void fps_camera_init(fps_camera* camera);
void fps_camera_update(fps_camera* camera, f32 dt);
void fps_camera_input(fps_camera* camera, f32 dt);
void fps_camera_resize(fps_camera* camera, i32 width, i32 height);
void fps_camera_update_frustum(fps_camera* camera);

#endif