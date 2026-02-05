#pragma once
#include "browser.h"
#include "player.h"
#include "terminal.h"

class ActuallyGoodMP
{
public:
    ActuallyGoodMP() = default;
    void init();
    void run();
    void shutdown();

    void init_browsers();

private:
    // todo: add init_logging();

private:
    Browser _artist_browser;
    Browser _album_browser;
    Browser _song_browser;
    app_config _config;
    Player _player;
    Terminal _terminal;

};
