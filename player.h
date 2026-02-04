#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int agmp_play_file(const char* path);
int agmp_start_playback(const char* path);
void agmp_stop_playback();
void agmp_toggle_pause();
int agmp_is_done();
int agmp_get_position_ms();
void agmp_seek_ms(int position_ms);

#ifdef __cplusplus
}
#endif
