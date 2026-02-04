#pragma once
#include "browser.h"
#include "config.h"
#include "player.h"

class ActuallyGoodMP
{
public:
    ActuallyGoodMP();
    void init();
    void run();
    void shutdown();

private:
    // todo: add init_logging();

private:
    Browser _artist_browser;
    Browser _album_browser;
    Browser _song_browser;
    app_config _config;
    Player _player;

};
