#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "player.h"

#include <stdio.h>

static ma_engine g_engine;
static ma_sound g_sound;
static int g_initialized = 0;
static int g_paused = 0;

int agmp_play_file(const char* path) {
    int result = agmp_start_playback(path);
    if (result != 0) {
        return result;
    }

    while (!agmp_is_done()) {
        ma_sleep(100);
    }

    agmp_stop_playback();
    return 0;
}

int agmp_start_playback(const char* path) {
    if (g_initialized) {
        return 0;
    }

    ma_result result = ma_engine_init(NULL, &g_engine);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize audio engine.\n");
        return 1;
    }

    result = ma_sound_init_from_file(&g_engine, path, 0, NULL, NULL, &g_sound);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to load audio file: %s\n", path);
        ma_engine_uninit(&g_engine);
        return 1;
    }

    ma_sound_start(&g_sound);
    g_initialized = 1;
    g_paused = 0;
    return 0;
}

void agmp_stop_playback() {
    if (!g_initialized) {
        return;
    }

    ma_sound_uninit(&g_sound);
    ma_engine_uninit(&g_engine);
    g_initialized = 0;
    g_paused = 0;
}

void agmp_toggle_pause() {
    if (!g_initialized) {
        return;
    }

    if (g_paused) {
        ma_sound_start(&g_sound);
        g_paused = 0;
    } else {
        ma_sound_stop(&g_sound);
        g_paused = 1;
    }
}

int agmp_is_done() {
    if (!g_initialized) {
        return 1;
    }
    return ma_sound_at_end(&g_sound) ? 1 : 0;
}

int agmp_get_position_ms() {
    if (!g_initialized) {
        return 0;
    }

    ma_uint64 frames = 0;
    if (ma_sound_get_cursor_in_pcm_frames(&g_sound, &frames) != MA_SUCCESS) {
        return 0;
    }

    ma_uint32 sample_rate = ma_engine_get_sample_rate(&g_engine);
    if (sample_rate == 0) {
        return 0;
    }

    ma_uint64 ms = (frames * 1000) / sample_rate;
    return (int)ms;
}

void agmp_seek_ms(int position_ms) {
    if (!g_initialized) {
        return;
    }

    if (position_ms < 0) {
        position_ms = 0;
    }

    ma_uint32 sample_rate = ma_engine_get_sample_rate(&g_engine);
    if (sample_rate == 0) {
        return;
    }

    ma_uint64 frames = ((ma_uint64)position_ms * sample_rate) / 1000;
    ma_sound_seek_to_pcm_frame(&g_sound, frames);
}
