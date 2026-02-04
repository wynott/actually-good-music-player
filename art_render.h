#pragma once

#include <string>
#include <vector>

#include "album_art.h"
#include "config.h"

struct art_result
{
    bool ready;
    bool has_art;
    bool online_failed;
    std::vector<unsigned char> data;
    std::string error_message;
};

art_result start_album_art_fetch(
    const char* path,
    const app_config& config,
    const std::string& artist,
    const std::string& album);

void complete_album_art_fetch(
    art_result& result,
    const app_config& config,
    const std::string& artist,
    const std::string& album);

bool render_album_art(
    const art_result& result,
    const app_config& config,
    int origin_x,
    int origin_y,
    app_config::rgb_color* out_avg_color);

class ArtRenderer
{
public:
    bool render(
        const art_result& result,
        const app_config& config,
        int origin_x,
        int origin_y,
        app_config::rgb_color* out_avg_color);

private:
    AlbumArt _album_art;
};
