#include "app.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#include "app.h"
#include "canvas.h"
#include "draw.h"
#include "event.h"
#include "http.h"
#include "input.h"
#include "metadata.h"
#include "net.h"
#include "player.h"
#include "spectrum_analyzer.h"
#include "state.h"
#include "terminal.h"
#include "rice.h"

#include <glm/vec4.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#if defined(_WIN32)
#include <windows.h>
#endif

static char normalize_key(char value)
{
    unsigned char ch = static_cast<unsigned char>(value);
    if (ch >= 'A' && ch <= 'Z')
    {
        return static_cast<char>(ch - 'A' + 'a');
    }
    return value;
}

ActuallyGoodMP& ActuallyGoodMP::instance()
{
    static ActuallyGoodMP app;
    return app;
}

static int map_navigation_key(const app_config& config, int key)
{
    if (key == -1)
    {
        return -1;
    }
    char ch = normalize_key(static_cast<char>(key));
    if (ch == normalize_key(config.nav_up_key))
    {
        return input_key_up;
    }
    if (ch == normalize_key(config.nav_down_key))
    {
        return input_key_down;
    }
    if (ch == normalize_key(config.nav_left_key))
    {
        return input_key_left;
    }
    if (ch == normalize_key(config.nav_right_key))
    {
        return input_key_right;
    }
    return key;
}

void ActuallyGoodMP::init()
{
    spdlog::drop("agmp");
    auto log = spdlog::basic_logger_mt("agmp", "agmp.log", true);
    spdlog::set_default_logger(log);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    spdlog::flush_on(spdlog::level::info);

    _terminal.init();

    http_init();
    input_init();

    _config = load_config("config.toml");
    if (_config.safe_mode)
    {
        _config.enable_online_art = false;
    }

    Renderer::init(_terminal);

    if (auto renderer = Renderer::get())
    {
        _canvas.resize(renderer->get_terminal_size());
        if (_config.draw_grid_canvas)
        {
            _canvas.build_grid(_config);
        }
        else
        {
            _canvas.build_default(_config);
        }
        renderer->set_canvas(_canvas.get_buffer());
    }

    _mp3_selected_subscription = EventBus::instance().subscribe(
        "browser.mp3_selected",
        [this](const Event& event)
        {
            if (event.payload.empty())
            {
                return;
            }

            _player.set_current_track(event.payload);
            _player.stop_playback();
            _player.start_playback(_player.get_current_track());

            _player.ensure_context_from_track();
            auto context = _player.get_context();
            _album_art.set_track(_player.get_current_track(), _config, context.artist, context.album);
            _album_art.refresh(_config, 0, 0);
            update_canvas_from_album();

            int scrubber_columns = std::max(1, _config.scrubber_width - 2);
            _scrubber.request_waveform(_player.get_current_track(), scrubber_columns);
        });

    _album_art_subscription = EventBus::instance().subscribe(
        "album_art.updated",
        [this](const Event&)
        {
            _album_art.refresh(_config, 0, 0);
            update_canvas_from_album();
        });

    _queue_subscription = EventBus::instance().subscribe(
        "queue.enqueue",
        [this](const Event& event)
        {
            if (event.payload.empty())
            {
                return;
            }

            _queue.enqueue(event.payload);
            _queue.draw(_config);
        });

    _stop_play_subscription = EventBus::instance().subscribe(
        "player.stop",
        [this](const Event&)
        {
            _player.stop_playback();
        });

    init_browsers();
}

void ActuallyGoodMP::run()
{
    _rice.run(_config);
    
    MetadataPanel metadata_panel;

    SpectrumAnalyzer analyzer;

    int metadata_origin_x = _config.metadata_origin_x;
    int metadata_origin_y = _config.metadata_origin_y;
    int metadata_width = _config.metadata_width;
    int metadata_height = _config.metadata_height;
    metadata_panel.set_location(glm::ivec2(metadata_origin_x, metadata_origin_y));
    metadata_panel.set_size(glm::ivec2(metadata_width, metadata_height));

    _queue.set_location(glm::ivec2(_config.queue_origin_x, _config.queue_origin_y));
    _queue.set_size(glm::ivec2(_config.queue_width, _config.queue_height));

    _scrubber.set_location(glm::ivec2(_config.scrubber_origin_x, _config.scrubber_origin_y));
    _scrubber.set_size(glm::ivec2(_config.scrubber_width, _config.scrubber_height));

    analyzer.set_location(glm::ivec2(_config.spectrum_origin_x, _config.spectrum_origin_y));
    analyzer.set_size(glm::ivec2(_config.spectrum_width, _config.spectrum_height));
    _player.set_spectrum_analyzer(&analyzer);
    _player.set_queue(&_queue);

    _artist_browser.draw();
    _album_browser.draw();
    _song_browser.draw();
    _action_browser.draw();
    _queue.draw(_config);
    _scrubber.draw(_config);
    if (!_config.safe_mode)
    {
        track_metadata meta;
        if (read_track_metadata(_player.get_current_track(), meta))
        {
            metadata_panel.draw(_config, meta);
        }
    }
    _terminal.mark_all_dirty();
    _terminal.update();


    net_info info;
    bool network_started = start_network(_config.listen_port, info);
    std::string status_base;
    if (network_started)
    {
        status_base = "IP: " + info.ip_address + ":" + std::to_string(info.port);
        if (auto renderer = Renderer::get())
        {
            renderer->draw_string(status_base, glm::ivec2(1, 1));
        }
    }

    app_state state = load_state("state.toml");
    if (_config.auto_resume_playback && !state.context.track_path.empty())
    {
        _player.set_context(state.context);
    }

    state.apply_to_browsers(_artist_browser, _album_browser, _song_browser);
    _queue.set_paths(state.queue_paths);
    _queue.draw(_config);

    _player.ensure_context_from_track();
    player_context context = _player.get_context();
    _album_art.set_track(_player.get_current_track(), _config, context.artist, context.album);
    _album_art.refresh(_config, 0, 0);
    update_canvas_from_album();

    _scrubber.request_waveform(_player.get_current_track(), std::max(1, _config.scrubber_width - 2));


    _artist_browser.draw();
    _album_browser.draw();
    _song_browser.draw();
    _action_browser.draw();
    _queue.draw(_config);
    _scrubber.draw(_config);
 
    if (!_config.safe_mode)
    {
        track_metadata initial_meta;
        if (read_track_metadata(_player.get_current_track(), initial_meta) && initial_meta.duration_ms > 0)
        {
            if (state.context.position_ms >= initial_meta.duration_ms)
            {
                state.context.position_ms = 0;
            }
        }
    }

    bool playback_active = _player.start_playback(_player.get_current_track()) == 0;
    if (_config.auto_resume_playback && playback_active
        && state.context.position_ms > 0
        && _player.get_current_track() == state.context.track_path)
    {
        _player.seek_ms(state.context.position_ms);
    }

    using clock = std::chrono::steady_clock;
    auto last_frame = clock::now();
    double delta_time = 0.0;
    double smoothed_fps = 0.0;
    int target_rate = std::max(1, _config.target_refresh_rate);
    double target_frame_time = 1.0 / static_cast<double>(target_rate);

    _terminal.mark_all_dirty();
    bool quit = false;
    while (!quit)
    {
        auto frame_start = clock::now();
        delta_time = std::chrono::duration<double>(frame_start - last_frame).count();
        last_frame = frame_start;
        if (delta_time > 0.0)
        {
            double fps = 1.0 / delta_time;
            if (smoothed_fps <= 0.0)
            {
                smoothed_fps = fps;
            }
            else
            {
                smoothed_fps = smoothed_fps * 0.9 + fps * 0.1;
            }
        }

        if (playback_active && _player.is_done())
        {
            _player.handle_track_finished();
        }

        if (!_config.safe_mode)
        {
            track_metadata meta;
            if (read_track_metadata(_player.get_current_track(), meta))
            {
                metadata_panel.draw(_config, meta);
                if (meta.duration_ms > 0)
                {
                    float progress = static_cast<float>(_player.get_position_ms()) / static_cast<float>(meta.duration_ms);
                    _scrubber.set_progress(progress);
                    _scrubber.set_time_ms(_player.get_position_ms(), meta.duration_ms);
                }
            }
        }

        _scrubber.draw(_config);

        analyzer.update();
        analyzer.draw();

        glm::ivec2 term_size = _terminal.get_size();
        int fps_value = static_cast<int>(smoothed_fps + 0.5);
        std::string fps_text = std::to_string(fps_value) + " fps";
        int fps_x = term_size.x - static_cast<int>(fps_text.size());

        if (fps_x < 0)
        {
            fps_x = 0;
        }

        if (auto renderer = Renderer::get())
        {
            renderer->draw_string(fps_text, glm::ivec2(fps_x, 0));
        }

        int raw_key = input_poll_key();
        int key = raw_key;
        if (!_config.use_arrow_keys)
        {
            if (raw_key == input_key_up || raw_key == input_key_down || raw_key == input_key_left || raw_key == input_key_right)
            {
                key = -1;
            }
        }
        key = map_navigation_key(_config, key);
        if (key != -1)
        {
            char ch = normalize_key(static_cast<char>(key));
            char pause_key = normalize_key(_config.play_pause_key);
            char quit_key = normalize_key(_config.quit_key);
            if (ch == pause_key)
            {
                _player.toggle_pause();
            }
            if (ch == quit_key)
            {
                quit = true;
            }

            _artist_browser.update(key);
        }

        _terminal.update();
        auto frame_end = clock::now();
        double frame_time = std::chrono::duration<double>(frame_end - frame_start).count();
        double sleep_time = target_frame_time - frame_time;
        if (sleep_time > 0.0)
        {
            std::this_thread::sleep_for(std::chrono::duration<double>(sleep_time));
        }
    }

    return;
}

const app_config& ActuallyGoodMP::get_config() const
{
    return _config;
}

Canvas* ActuallyGoodMP::get_canvas()
{
    return &_canvas;
}

void ActuallyGoodMP::shutdown()
{
    app_state save;
    save.context = _player.get_context();
    save.context.position_ms = _player.get_position_ms();
    save.artist_path = _artist_browser.get_path().string();
    save.album_path = _album_browser.get_path().string();
    save.song_path = _song_browser.get_path().string();
    save.song_index = static_cast<int>(_song_browser.get_selected_index());
    save.queue_paths = _queue.get_paths();
    save_state("state.toml", save);

    _player.stop_playback();

    _terminal.shutdown();

    if (_mp3_selected_subscription != 0)
    {
        EventBus::instance().unsubscribe(_mp3_selected_subscription);
        _mp3_selected_subscription = 0;
    }

    if (_album_art_subscription != 0)
    {
        EventBus::instance().unsubscribe(_album_art_subscription);
        _album_art_subscription = 0;
    }

    if (_queue_subscription != 0)
    {
        EventBus::instance().unsubscribe(_queue_subscription);
        _queue_subscription = 0;
    }

    if (_stop_play_subscription != 0)
    {
        EventBus::instance().unsubscribe(_stop_play_subscription);
        _stop_play_subscription = 0;
    }
    
    input_shutdown();
    http_cleanup();
    stop_network();
}

void ActuallyGoodMP::update_canvas_from_album()
{
    auto renderer = Renderer::get();
    if (!renderer)
    {
        return;
    }

    _canvas.resize(renderer->get_terminal_size());
    if (_config.draw_grid_canvas)
    {
        _canvas.build_grid(_config);
    }
    else
    {
        _canvas.build_from_album(_config, _album_art);
    }
    renderer->set_canvas(_canvas.get_buffer());
}

void ActuallyGoodMP::init_browsers()
{
    int browser_gap = _config.browser_padding;
    glm::ivec2 browser_origin(2, 3);
    glm::ivec2 artist_browser_size(std::max(1, _config.col_width_artist + 2), 12);
    glm::ivec2 album_browser_size(std::max(1, _config.col_width_album + 2), 12);
    glm::ivec2 song_browser_size(std::max(1, _config.col_width_song + 2), 12);
    glm::ivec2 action_browser_size(std::max(1, _config.col_width_song + 2), 6);

    _artist_browser.set_name("Artist");
    _artist_browser.set_location(browser_origin);
    _artist_browser.set_size(artist_browser_size);
    _album_browser.set_name("Album");
    _album_browser.set_location(glm::ivec2(browser_origin.x + artist_browser_size.x + browser_gap, browser_origin.y));
    _album_browser.set_size(album_browser_size);
    _song_browser.set_name("Song");
    _song_browser.set_location(glm::ivec2(
       browser_origin.x + artist_browser_size.x + browser_gap + album_browser_size.x + browser_gap,
       browser_origin.y));
    _song_browser.set_size(song_browser_size);

    _action_browser.set_name("Actions");
    _action_browser.set_location(glm::ivec2(
        _song_browser.get_location().x + song_browser_size.x + browser_gap,
        _song_browser.get_location().y));
    _action_browser.set_size(action_browser_size);

    _artist_browser.set_right(&_album_browser);
    _album_browser.set_left(&_artist_browser);
    _album_browser.set_right(&_song_browser);
    _song_browser.set_left(&_album_browser);
    _song_browser.set_right(&_action_browser);
    _action_browser.set_left(&_song_browser);

    _artist_browser.set_focused(true);
    
    _player.set_song_browser(&_song_browser);

    _artist_browser.set_path(_config.library_path);
    _action_browser.refresh();
}
