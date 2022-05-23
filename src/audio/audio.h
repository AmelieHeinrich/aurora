#pragma once

#include <core/common.h>
#include <core/platform_layer.h>

#include <miniaudio.h>
#include <dr_wav.h>

typedef struct AudioClip AudioClip;
struct AudioClip
{
    ma_decoder decoder;
    ma_decoder_config decoder_config;

    b32 playing;
    b32 loop;

    u32 id;
    drwav wav;
};

void audio_init();
void audio_exit();
void audio_update();
void audio_async_update(Thread* thread);

void audio_clip_load_wav(AudioClip* clip, const char* path);
void audio_clip_free(AudioClip* clip);
void audio_clip_play(AudioClip* clip);
void audio_clip_stop(AudioClip* clip);
void audio_clip_loop(AudioClip* clip, b32 loop);