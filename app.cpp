#include "app.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "album_art.h"
#include "draw.h"
#include "http.h"
#include "input.h"
#include "metadata.h"
#include "net.h"
#include "player.h"
#include "spectrum_analyzer.h"
#include "state.h"
#include "terminal.h"
#include "rice.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#if defined(_WIN32)
#include <windows.h>
#endif

ActuallyGoodMP::ActuallyGoodMP()
{
    //_album_browser.set location  etc
    //_artist_browser path = from config
}

static char normalize_key(char value)
{
    unsigned char ch = static_cast<unsigned char>(value);
    if (ch >= 'A' && ch <= 'Z')
    {
        return static_cast<char>(ch - 'A' + 'a');
    }
    return value;
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

#if defined(_WIN32)
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE)
    {
        SetConsoleOutputCP(CP_UTF8);
        DWORD mode = 0;
        if (GetConsoleMode(handle, &mode))
        {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
            SetConsoleMode(handle, mode);
        }
    }
#endif

    http_init();
    input_init();

    _config = load_config("config.toml");
    if (_config.safe_mode)
    {
        _config.enable_online_art = false;
    }

    int browser_gap = _config.browser_padding;
    glm::ivec2 browser_origin(2, 3);
    glm::ivec2 artist_browser_size(std::max(1, _config.col_width_artist + 2), 12);
    glm::ivec2 album_browser_size(std::max(1, _config.col_width_album + 2), 12);
    glm::ivec2 song_browser_size(std::max(1, _config.col_width_song + 2), 12);

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

    _artist_browser.set_right(&_album_browser);
    _album_browser.set_left(&_artist_browser);
    _album_browser.set_right(&_song_browser);
    _song_browser.set_left(&_album_browser);
    _artist_browser.set_focused(true);
    _artist_browser.set_path(_config.library_path);
    _song_browser.set_player(&_player);
    _player.set_song_browser(&_song_browser);

    Renderer::instance().set_canvas(glm::vec3(0.0f));
    Renderer::instance().set_box_colour(glm::vec3(
        static_cast<float>(_config.ui_box_fg.r),
        static_cast<float>(_config.ui_box_fg.g),
        static_cast<float>(_config.ui_box_fg.b)));
    Renderer::instance().set_text_colour(glm::vec3(
        static_cast<float>(_config.ui_text_fg.r),
        static_cast<float>(_config.ui_text_fg.g),
        static_cast<float>(_config.ui_text_fg.b)));
    {
        glm::ivec2 size = Terminal::instance().get_size();
        if (size.x > 0 && size.y > 0)
        {
            Terminal::instance().write_region(glm::ivec2(0, 0), glm::ivec2(size.x - 1, size.y - 1));
        }
    }
}

void ActuallyGoodMP::run()
{
    Rice rice;
    rice.run(_config);

    AlbumArt album_art;
    MetadataPanel metadata_panel;
    SpectrumAnalyzer analyzer;
    album_art.set_player(&_player);

    int metadata_origin_x = _config.art_width_chars + 2;
    int metadata_width = _config.metadata_max_width > 0 ? _config.metadata_max_width + 2 : 32;
    metadata_panel.set_location(glm::ivec2(metadata_origin_x + 1, 3));
    metadata_panel.set_size(glm::ivec2(metadata_width, 12));

    analyzer.set_location(glm::ivec2(metadata_origin_x + 1, 16));
    analyzer.set_size(glm::ivec2(metadata_width, 8));
    analyzer.set_bar_colour(glm::vec3(
        static_cast<float>(_config.ui_text_fg.r),
        static_cast<float>(_config.ui_text_fg.g),
        static_cast<float>(_config.ui_text_fg.b)));
    _player.set_spectrum_analyzer(&analyzer);

    net_info info;
    bool network_started = start_network(_config.listen_port, info);
    std::string status_base;
    if (network_started)
    {
        status_base = "IP: " + info.ip_address + ":" + std::to_string(info.port);
        draw_status_line(status_base);
    }

    app_state state = load_state("state.toml");
    if (_config.auto_resume_playback && !state.context.track_path.empty())
    {
        _player.set_context(state.context);
    }

    state.apply_to_browsers(_artist_browser, _album_browser, _song_browser);

    _player.ensure_context_from_track();
    player_context context = _player.get_context();
    album_art.set_track(_player.get_current_track(), _config, context.artist, context.album);


    _artist_browser.draw();
    _album_browser.draw();
    _song_browser.draw();
 
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

    bool quit = false;
    while (!quit)
    {
        if (playback_active && _player.is_done())
        {
            _player.handle_track_finished();
        }

        album_art.update_from_player(_config, 0, 0);
        analyzer.update();
        analyzer.draw();

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

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return;
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
    save_state("state.toml", save);
    _player.stop_playback();
    std::cout << "\x1b[2J\x1b[H\x1b[0m";
    std::cout.flush();
    input_shutdown();
    http_cleanup();
    stop_network();
}
