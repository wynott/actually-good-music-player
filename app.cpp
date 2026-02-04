#include "app.h"

#include <chrono>
#include <iostream>
#include <string>

#include "album_art.h"
#include "browser.h"
#include "config.h"
#include "draw.h"
#include "http.h"
#include "input.h"
#include "metadata.h"
#include "net.h"
#include "player.h"
#include "state.h"
#include "terminal.h"

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

void ActuallyGoodMP::run()
{
    try
    {
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

        app_config config = load_config("config.toml");
        if (config.safe_mode)
        {
            config.enable_online_art = false;
        }
        const char* default_track = config.default_track.c_str();

        int experimental_gap = 6;
        int experimental_offset = 25;
        glm::ivec2 experimental_origin(experimental_offset, experimental_offset);
        glm::ivec2 experimental_artist_size(std::max(1, config.col_width_artist + 2), 12);
        glm::ivec2 experimental_album_size(std::max(1, config.col_width_album + 2), 12);
        glm::ivec2 experimental_song_size(std::max(1, config.col_width_song + 2), 12);
        Browser experimental_artist_browser(
            "Artist",
            config.library_path,
            experimental_origin,
            experimental_artist_size);
        Browser experimental_album_browser(
            "Album",
            config.library_path,
            glm::ivec2(experimental_origin.x + experimental_artist_size.x + experimental_gap, experimental_origin.y),
            experimental_album_size);
        Browser experimental_song_browser(
            "Song",
            config.library_path,
            glm::ivec2(
                experimental_origin.x + experimental_artist_size.x + experimental_gap + experimental_album_size.x + experimental_gap,
                experimental_origin.y),
            experimental_song_size);
        experimental_artist_browser.set_right(&experimental_album_browser);
        experimental_album_browser.set_left(&experimental_artist_browser);
        experimental_album_browser.set_right(&experimental_song_browser);
        experimental_song_browser.set_left(&experimental_album_browser);
        experimental_artist_browser.set_focused(true);
        experimental_artist_browser.soft_select();

        Renderer::instance().set_canvas(glm::vec3(0.0f));
        Renderer::instance().set_box_colour(glm::vec3(
            static_cast<float>(config.ui_box_fg.r),
            static_cast<float>(config.ui_box_fg.g),
            static_cast<float>(config.ui_box_fg.b)));
        Renderer::instance().set_text_colour(glm::vec3(
            static_cast<float>(config.ui_text_fg.r),
            static_cast<float>(config.ui_text_fg.g),
            static_cast<float>(config.ui_text_fg.b)));
        {
            glm::ivec2 size = Terminal::instance().get_size();
            if (size.x > 0 && size.y > 0)
            {
                Terminal::instance().write_region(glm::ivec2(0, 0), glm::ivec2(size.x - 1, size.y - 1));
            }
        }
        AlbumArt album_art;
        Player player;

        int art_origin_x = 0;
        int metadata_origin_x = config.art_width_chars + 2;

        net_info info;
        bool network_started = start_network(config.listen_port, info);
        std::string status_base;
        if (network_started)
        {
            status_base = "IP: " + info.ip_address + ":" + std::to_string(info.port);
            draw_status_line(status_base);
        }

        player.set_current_track(default_track);
        app_state state = load_state("state.toml");
        if (config.auto_resume_playback && !state.last_track.empty())
        {
            player.set_current_track(state.last_track);
        }

        std::string current_artist;
        std::string current_album;
        try
        {
            std::filesystem::path track_path(player.get_current_track());
            if (track_path.has_parent_path())
            {
                current_album = track_path.parent_path().filename().string();
                if (track_path.parent_path().has_parent_path())
                {
                    current_artist = track_path.parent_path().parent_path().filename().string();
                }
            }
        }
        catch (...)
        {
        }

        album_art.begin_fetch(player.get_current_track(), config, current_artist, current_album);

        auto full_redraw = [&]()
        {
            app_config::rgb_color avg_color = config.browser_normal_bg;
            if (config.safe_mode)
            {
                draw_frame();
            }
            else
            {
                bool rendered = album_art.render_current(
                    config,
                    art_origin_x,
                    0,
                    &avg_color);
                if (!rendered)
                {
                    draw_frame();
                }
                config.browser_normal_bg = avg_color;
            }
            experimental_artist_browser.draw();
            experimental_album_browser.draw();
            experimental_song_browser.draw();
            {
                glm::ivec2 min_corner = experimental_artist_browser.get_location();
                glm::ivec2 max_corner = min_corner + experimental_artist_browser.get_size() - glm::ivec2(1);
                Terminal::instance().write_region(min_corner, max_corner);
            }
            {
                glm::ivec2 min_corner = experimental_album_browser.get_location();
                glm::ivec2 max_corner = min_corner + experimental_album_browser.get_size() - glm::ivec2(1);
                Terminal::instance().write_region(min_corner, max_corner);
            }
            {
                glm::ivec2 min_corner = experimental_song_browser.get_location();
                glm::ivec2 max_corner = min_corner + experimental_song_browser.get_size() - glm::ivec2(1);
                Terminal::instance().write_region(min_corner, max_corner);
            }
            if (!config.safe_mode)
            {
                track_metadata meta;
                if (read_track_metadata(player.get_current_track(), meta))
                {
                    draw_metadata_panel(config, meta, metadata_origin_x + 1, 3);
                }
            }
            if (!status_base.empty())
            {
                draw_status_line(status_base);
            }
        };

        if (!album_art.is_pending())
        {
            full_redraw();
        }
        else
        {
            experimental_artist_browser.draw();
            experimental_album_browser.draw();
            experimental_song_browser.draw();
            {
                glm::ivec2 min_corner = experimental_artist_browser.get_location();
                glm::ivec2 max_corner = min_corner + experimental_artist_browser.get_size() - glm::ivec2(1);
                Terminal::instance().write_region(min_corner, max_corner);
            }
            {
                glm::ivec2 min_corner = experimental_album_browser.get_location();
                glm::ivec2 max_corner = min_corner + experimental_album_browser.get_size() - glm::ivec2(1);
                Terminal::instance().write_region(min_corner, max_corner);
            }
            {
                glm::ivec2 min_corner = experimental_song_browser.get_location();
                glm::ivec2 max_corner = min_corner + experimental_song_browser.get_size() - glm::ivec2(1);
                Terminal::instance().write_region(min_corner, max_corner);
            }
            if (!config.safe_mode)
            {
                track_metadata meta;
                if (read_track_metadata(player.get_current_track(), meta))
                {
                    draw_metadata_panel(config, meta, metadata_origin_x + 1, 3);
                }
            }
            if (!status_base.empty())
            {
                draw_status_line(status_base);
            }
        }

        if (!config.safe_mode)
        {
            track_metadata initial_meta;
            if (read_track_metadata(player.get_current_track(), initial_meta) && initial_meta.duration_ms > 0)
            {
                if (state.last_position_ms >= initial_meta.duration_ms)
                {
                    state.last_position_ms = 0;
                }
            }
        }

        auto start_playback_or_fallback = [&]() -> bool
        {
            if (player.start_playback(player.get_current_track()) == 0)
            {
                return true;
            }

            if (player.get_current_track() != default_track)
            {
                player.set_current_track(default_track);
                current_artist.clear();
                current_album.clear();
                try
                {
                    std::filesystem::path track_path(player.get_current_track());
                    if (track_path.has_parent_path())
                    {
                        current_album = track_path.parent_path().filename().string();
                        if (track_path.parent_path().has_parent_path())
                        {
                            current_artist = track_path.parent_path().parent_path().filename().string();
                        }
                    }
                }
                catch (...)
                {
                }

                return player.start_playback(player.get_current_track()) == 0;
            }

            return false;
        };

        bool playback_active = start_playback_or_fallback();
        bool playback_failed = !playback_active;
        if (config.auto_resume_playback && playback_active && state.last_position_ms > 0 && player.get_current_track() == state.last_track)
        {
            player.seek_ms(state.last_position_ms);
        }
        if (playback_failed)
        {
            draw_status_line("Playback failed");
        }

        input_init();
        bool quit = false;
        bool dirty = false;
        while (!quit)
        {
            if (playback_active && player.is_done())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            int raw_key = input_poll_key();
            int key = raw_key;
            if (!config.use_arrow_keys)
            {
                if (raw_key == input_key_up || raw_key == input_key_down || raw_key == input_key_left || raw_key == input_key_right)
                {
                    key = -1;
                }
            }
            key = map_navigation_key(config, key);
            if (key != -1)
            {
                char ch = normalize_key(static_cast<char>(key));
                char pause_key = normalize_key(config.play_pause_key);
                char quit_key = normalize_key(config.quit_key);
                if (ch == pause_key)
                {
                    player.toggle_pause();
                }
                if (ch == quit_key)
                {
                    quit = true;
                }

                if (key == input_key_left)
                {
                    Browser* focused = experimental_artist_browser.get_focused_in_chain();

                    if (focused)
                    {
                        Browser* left = focused->get_left();
                        if (left)
                        {
                            focused->give_focus(left);
                            focused->draw();
                            left->draw();
                            {
                                glm::ivec2 min_corner = focused->get_location();
                                glm::ivec2 max_corner = min_corner + focused->get_size() - glm::ivec2(1);
                                Terminal::instance().write_region(min_corner, max_corner);
                            }
                            {
                                glm::ivec2 min_corner = left->get_location();
                                glm::ivec2 max_corner = min_corner + left->get_size() - glm::ivec2(1);
                                Terminal::instance().write_region(min_corner, max_corner);
                            }
                        }
                    }

                }
                if (key == input_key_right)
                {
                    Browser* focused = experimental_artist_browser.get_focused_in_chain();

                    if (focused)
                    {
                        Browser* right = focused->get_right();
                        if (right)
                        {
                            focused->give_focus(right);
                            focused->draw();
                            right->draw();
                            {
                                glm::ivec2 min_corner = focused->get_location();
                                glm::ivec2 max_corner = min_corner + focused->get_size() - glm::ivec2(1);
                                Terminal::instance().write_region(min_corner, max_corner);
                            }
                            {
                                glm::ivec2 min_corner = right->get_location();
                                glm::ivec2 max_corner = min_corner + right->get_size() - glm::ivec2(1);
                                Terminal::instance().write_region(min_corner, max_corner);
                            }
                        }
                    }

                }
                if (key == input_key_up || key == input_key_down)
                {
                    Browser* focused_browser = nullptr;
                    if (experimental_artist_browser.is_focused())
                    {
                        focused_browser = &experimental_artist_browser;
                    }
                    else if (experimental_album_browser.is_focused())
                    {
                        focused_browser = &experimental_album_browser;
                    }

                    if (focused_browser != nullptr)
                    {
                        int direction = (key == input_key_up) ? -1 : 1;
                        focused_browser->move_selection(direction);
                        focused_browser->draw();
                        glm::ivec2 min_corner = focused_browser->get_location();
                        glm::ivec2 max_corner = min_corner + focused_browser->get_size() - glm::ivec2(1);
                        Terminal::instance().write_region(min_corner, max_corner);

                        Browser* right_browser = focused_browser->get_right();
                        if (right_browser != nullptr)
                        {
                            right_browser->draw();
                            glm::ivec2 right_min = right_browser->get_location();
                            glm::ivec2 right_max = right_min + right_browser->get_size() - glm::ivec2(1);
                            Terminal::instance().write_region(right_min, right_max);
                        }
                    }


                }
            }

        if (album_art.consume_dirty())
        {
            full_redraw();
        }

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        input_shutdown();
        album_art.wait_for_fetch();
        http_cleanup();
        app_state save;
        save.last_track = player.get_current_track();
        save.last_position_ms = player.get_position_ms();
        save_state("state.toml", save);
        player.stop_playback();
        stop_network();
        std::cout << "\x1b[2J\x1b[H\x1b[0m";
        std::cout.flush();
    }
    catch (const std::exception& ex)
    {
        std::cout << "Fatal error: " << ex.what() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    catch (...)
    {
        std::cout << "Fatal error: unknown" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
