#pragma once

#include "browser.h"
#include "player.h"
#include "terminal.h"

class ActuallyGoodMP
{
public:
    static ActuallyGoodMP& instance();

    void init();
    void run();
    void shutdown();

    ActuallyGoodMP(const ActuallyGoodMP&) = delete;
    ActuallyGoodMP& operator=(const ActuallyGoodMP&) = delete;
    ActuallyGoodMP(ActuallyGoodMP&&) = delete;
    ActuallyGoodMP& operator=(ActuallyGoodMP&&) = delete;

    void init_browsers();

private:
    ActuallyGoodMP() = default;

    // todo: add init_logging();

private:
    Browser _artist_browser;
    Browser _album_browser;
    Browser _song_browser;
    app_config _config;
    Player _player;
    Terminal _terminal;
    int _mp3_selected_subscription = 0;

};
