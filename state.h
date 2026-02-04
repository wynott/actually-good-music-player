#pragma once

#include <string>

#include "player.h"

class Browser;

struct app_state
{
    player_context context;
    std::string artist_path;
    std::string album_path;
    std::string song_path;
    int song_index;

    void apply_to_browsers(Browser& artist, Browser& album, Browser& song) const;
};

app_state load_state(const std::string& path);
void save_state(const std::string& path, const app_state& state);
