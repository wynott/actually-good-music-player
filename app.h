#pragma once
#pragma once

#include "album_art.h"
#include "browser.h"
#include "canvas.h"
#include "player.h"
#include "queue.h"
#include "rice.h"
#include "scrubber.h"
#include "terminal.h"
#include "particles.h"

class ActuallyGoodMP
{
public:
    static ActuallyGoodMP& instance();

    void init();
    void run();
    void shutdown();

    const app_config& get_config() const;
    Canvas* get_canvas();

    ActuallyGoodMP(const ActuallyGoodMP&) = delete;
    ActuallyGoodMP& operator=(const ActuallyGoodMP&) = delete;
    ActuallyGoodMP(ActuallyGoodMP&&) = delete;
    ActuallyGoodMP& operator=(ActuallyGoodMP&&) = delete;

    void init_browsers();

private:
    void update_canvas_from_album();

private:
    ActuallyGoodMP() = default;

    // todo: add init_logging();

private:
    Browser _artist_browser;
    Browser _album_browser;
    Browser _song_browser;
    Browser _action_browser;
    AlbumArt _album_art;
    app_config _config;
    Player _player;
    Terminal _terminal;
    int _mp3_selected_subscription = 0;
    int _album_art_subscription = 0;
    int _album_art_online_subscription = 0;
    int _track_changed_subscription = 0;
    int _queue_subscription = 0;
    int _stop_play_subscription = 0;
    int _play_rest_subscription = 0;
    Canvas _canvas;
    Rice _rice;
    Queue _queue;
    Scrubber _scrubber;
    ParticleSystem _particles;

};
