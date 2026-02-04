#pragma once

#include <string>

struct app_config
{
    std::string default_track;
    std::string library_path;
    char play_pause_key;
    char quit_key;
    char nav_up_key;
    char nav_down_key;
    char nav_left_key;
    char nav_right_key;
    bool use_arrow_keys;
    bool enable_online_art;
    char search_key;
    bool auto_resume_playback;
    bool safe_mode;
    struct rgb_color
    {
        int r;
        int g;
        int b;
    };
    rgb_color browser_normal_fg;
    rgb_color browser_normal_bg;
    rgb_color browser_selected_fg;
    rgb_color browser_selected_bg;
    rgb_color browser_inactive_fg;
    rgb_color browser_inactive_bg;
    rgb_color ui_box_fg;
    rgb_color ui_text_fg;
    rgb_color rice_colour;
    rgb_color rice_background_tl;
    rgb_color rice_background_tr;
    rgb_color rice_background_bl;
    rgb_color rice_background_br;
    int metadata_max_width;
    int col_width_artist;
    int col_width_album;
    int col_width_song;
    int art_width_chars;
    int art_height_chars;
    int browser_padding;
    int listen_port;
};

app_config load_config(const std::string& path);
