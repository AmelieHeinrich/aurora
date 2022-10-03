#include "audio.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#define SAMPLE_FORMAT ma_format_s16;
#define CHANNEL_COUNT 2
#define SAMPLE_RATE 48000
#define MAX_AUDIO_CLIPS 1024

typedef struct AudioCtx AudioCtx;
struct AudioCtx
{
    ma_device device;
    ma_device_config device_config;

    AudioClip* clips[MAX_AUDIO_CLIPS];
    u32 clip_count;
};

AudioCtx ctx;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    /* Assuming format is always s16 for now. */
    for (i32 i = 0; i < ctx.clip_count; i++) {
        if (pDevice->playback.format == ma_format_s16) {
       	    drwav_read_pcm_frames_s16(&ctx.clips[i]->wav, frameCount, (drwav_int16*)pOutput);
    } else if (pDevice->playback.format == ma_format_f32) {
        drwav_read_pcm_frames_f32(&ctx.clips[i]->wav, frameCount, (float*)pOutput);
    } else {
    	    /* Unsupported format. */
        }
    }

    (void)pInput;
}

void audio_init()
{
    ctx.device_config = ma_device_config_init(ma_device_type_playback);
    ctx.device_config.playback.format = SAMPLE_FORMAT;
    ctx.device_config.playback.channels = CHANNEL_COUNT;
    ctx.device_config.sampleRate = SAMPLE_RATE;
    ctx.device_config.dataCallback = data_callback;
    ctx.device_config.pUserData = NULL;

    ma_device_init(NULL, &ctx.device_config, &ctx.device);
    ma_device_start(&ctx.device);
}

void audio_exit()
{
    ma_device_stop(&ctx.device);
    ma_device_uninit(&ctx.device);
}

void audio_update()
{
    for (u32 i = 0; i < ctx.clip_count; i++) {
        AudioClip* clip = ctx.clips[i];

	if (!clip->playing) {
	     audio_clip_stop(clip);
	     if (clip->loop) {
	         audio_clip_play(clip);
	     }
        }
    }
}

void audio_async_update(Thread* thread)
{
    while (aurora_platform_active_thread(thread))
    {
        for (u32 i = 0; i < ctx.clip_count; i++) {
	    AudioClip* clip = ctx.clips[i];
	
	    if (!clip->playing) {
	        audio_clip_stop(clip);
		if (clip->loop) {
		    audio_clip_play(clip);
		}
	    }
	}
    }
}

void audio_clip_load_wav(AudioClip* clip, const char* path)
{
    drwav_init_file(&clip->wav, path, NULL);
}

void audio_clip_free(AudioClip* clip)
{
    if (clip->playing) {
        audio_clip_stop(clip);
    }

    ma_decoder_uninit(&clip->decoder);

    drwav_uninit(&clip->wav);
}

void audio_clip_play(AudioClip* clip)
{
    for (u32 i = 0; i < ctx.clip_count; i++) {
        if (ctx.clips[i] == clip) {
	    audio_clip_stop(ctx.clips[i]);
	    break;
	}
    }

    ma_decoder_seek_to_pcm_frame(&clip->decoder, 0);

    clip->playing = 1;
    ctx.clips[ctx.clip_count] = clip;
    clip->id = ctx.clip_count++;
}

void audio_clip_stop(AudioClip* clip)
{
    clip->playing = 0;

    ctx.clips[clip->id] = ctx.clips[ctx.clip_count - 1];
    ctx.clips[clip->id]->id = clip->id;
    ctx.clip_count--;
}

void audio_clip_loop(AudioClip* clip, b32 loop)
{
    clip->loop = loop;
}
