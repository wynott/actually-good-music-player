#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <stdio.h>

int agmp_play_file(const char* path) {
    ma_engine engine;
    ma_result result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize audio engine.\n");
        return 1;
    }

    ma_sound sound;
    result = ma_sound_init_from_file(&engine, path, 0, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to load audio file: %s\n", path);
        ma_engine_uninit(&engine);
        return 1;
    }

    ma_sound_start(&sound);

    while (!ma_sound_at_end(&sound)) {
        ma_sleep(100);
    }

    ma_sound_uninit(&sound);
    ma_engine_uninit(&engine);
    return 0;
}
