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

static bool fuzzy_match_score(const std::string& candidate, const std::string& query, int& score)
{
    if (query.empty())
    {
        score = 0;
        return true;
    }

    int last_pos = -1;
    int gap = 0;
    size_t qi = 0;

    for (size_t i = 0; i < candidate.size() && qi < query.size(); ++i)
    {
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(candidate[i])));
        char q = static_cast<char>(std::tolower(static_cast<unsigned char>(query[qi])));
        if (c == q)
        {
            if (last_pos >= 0)
            {
                gap += static_cast<int>(i - last_pos - 1);
            }
            last_pos = static_cast<int>(i);
            qi += 1;
        }
    }

    if (qi != query.size())
    {
        return false;
    }

    score = gap;
    return true;
}

static int find_best_match(const std::vector<browser_column::entry>& entries, const std::string& query)
{
    int best_index = -1;
    int best_score = 0;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        int score = 0;
        if (fuzzy_match_score(entries[i].name, query, score))
        {
            if (best_index == -1 || score < best_score)
            {
                best_index = static_cast<int>(i);
                best_score = score;
            }
        }
    }
    return best_index;
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

        int box_gap = 2;
        int art_origin_x = 0;
        int metadata_origin_x = 0;

        browser_column artist;
        artist.path = config.library_path;
        artist.entries = list_entries(artist.path);
        artist.selected_index = artist.entries.empty() ? -1 : 0;
        artist.width = std::max(1, config.col_width_artist);
        artist.start_col = 2;

        browser_column album;
        album.selected_index = 0;
        album.width = std::max(1, config.col_width_album);
        album.start_col = 2;

        browser_column song;
        song.selected_index = 0;
        song.width = std::max(1, config.col_width_song);
        song.start_col = 2;

        if (artist.selected_index >= 0)
        {
            const auto& selected = artist.entries[artist.selected_index];
            if (selected.is_dir)
            {
                album.path = selected.path;
            }
            else
            {
                album.path.clear();
            }
            refresh_column(album);
        }
        else
        {
            album.path.clear();
            refresh_column(album);
        }

        if (album.selected_index >= 0)
        {
            const auto& selected = album.entries[album.selected_index];
            if (selected.is_dir)
            {
                song.path = selected.path;
            }
            else
            {
                song.path.clear();
            }
            refresh_column(song);
        }
        else
        {
            song.path.clear();
            refresh_column(song);
        }

        int active_column = 0;
        int prev_artist_rows = 0;
        int prev_album_rows = 0;
        int prev_song_rows = 0;
        update_layout(config, artist, album, song, box_gap, art_origin_x, metadata_origin_x);
        draw_browsers(config, artist, album, song, active_column);

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

        std::string search_query;
        bool search_active = false;

        album_art.begin_fetch(player.get_current_track(), config, current_artist, current_album);

        auto full_redraw = [&]()
        {
            update_layout(config, artist, album, song, box_gap, art_origin_x, metadata_origin_x);
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
            TerminalSize size = get_terminal_size();
            int start_row = 3;
            int max_rows = std::max(0, size.rows - start_row + 1);
            int artist_rows = std::min(max_rows, static_cast<int>(artist.entries.size()));
            int album_rows = std::min(max_rows, static_cast<int>(album.entries.size()));
            int song_rows = std::min(max_rows, static_cast<int>(song.entries.size()));
            fill_browser_area(config, artist, start_row, std::max(prev_artist_rows, artist_rows));
            fill_browser_area(config, album, start_row, std::max(prev_album_rows, album_rows));
            fill_browser_area(config, song, start_row, std::max(prev_song_rows, song_rows));
            draw_browsers(config, artist, album, song, active_column);
            prev_artist_rows = artist_rows;
            prev_album_rows = album_rows;
            prev_song_rows = song_rows;
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
                if (search_active)
                {
                    draw_status_line(status_base + " | /" + search_query);
                }
                else
                {
                    draw_status_line(status_base);
                }
            }
        };

        if (!album_art.is_pending())
        {
            full_redraw();
        }
        else
        {
            draw_browsers(config, artist, album, song, active_column);
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
                if (song.selected_index >= 0 && song.selected_index + 1 < static_cast<int>(song.entries.size()))
                {
                    song.selected_index += 1;
                    player.set_current_track(song.entries[song.selected_index].path.string());
                    current_album = album.selected_index >= 0 ? album.entries[album.selected_index].name : "";
                    current_artist = artist.selected_index >= 0 ? artist.entries[artist.selected_index].name : "";
                    player.stop_playback();
                    playback_active = player.start_playback(player.get_current_track()) == 0;
                    playback_failed = !playback_active;
                    if (playback_failed)
                    {
                        draw_status_line("Playback failed");
                    }
                    album_art.begin_fetch(player.get_current_track(), config, current_artist, current_album);
                    full_redraw();
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
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
                char search_key = normalize_key(config.search_key);

                if (!search_active)
                {
                    if (ch == pause_key)
                    {
                        player.toggle_pause();
                    }
                    if (ch == quit_key)
                    {
                        quit = true;
                    }
                    if (ch == search_key)
                    {
                        search_active = true;
                        search_query.clear();
                        if (!status_base.empty())
                        {
                            draw_status_line(status_base + " | /" + search_query);
                        }
                    }
                }
                else
                {
                    if (key == '\r' || key == '\n' || key == 27)
                    {
                        search_active = false;
                        full_redraw();
                    }
                    else if (key == 8 || key == 127)
                    {
                        if (!search_query.empty())
                        {
                            search_query.pop_back();
                        }
                    }
                    else if (std::isprint(static_cast<unsigned char>(key)))
                    {
                        search_query.push_back(static_cast<char>(key));
                    }

                    if (search_active)
                    {
                        int match = find_best_match(artist.entries, search_query);
                        if (match >= 0 && match != artist.selected_index)
                        {
                            int previous = artist.selected_index;
                            artist.selected_index = match;

                            TerminalSize size = get_terminal_size();
                            int start_row = 3;
                            int max_rows = std::max(0, size.rows - start_row + 1);
                            int artist_rows = std::min(max_rows, static_cast<int>(artist.entries.size()));
                            int album_rows = std::min(max_rows, static_cast<int>(album.entries.size()));
                            int song_rows = std::min(max_rows, static_cast<int>(song.entries.size()));
                            bool layout_changed = update_layout(config, artist, album, song, box_gap, art_origin_x, metadata_origin_x);
                            if (layout_changed)
                            {
                                full_redraw();
                                album_art.wait_for_fetch();
                            }
                            if (previous >= 0 && previous < artist_rows)
                            {
                                draw_browser_row(config, artist, previous, start_row, active_column == 0);
                            }
                            if (match < artist_rows)
                            {
                                draw_browser_row(config, artist, match, start_row, active_column == 0);
                            }

                            const auto& selected = artist.entries[artist.selected_index];
                            album.path = selected.is_dir ? selected.path : std::filesystem::path();
                            refresh_column(album);

                            if (album.selected_index >= 0)
                            {
                                const auto& album_selected = album.entries[album.selected_index];
                                song.path = album_selected.is_dir ? album_selected.path : std::filesystem::path();
                            }
                            else
                            {
                                song.path.clear();
                            }
                            refresh_column(song);

                            draw_browser_column(config, album, start_row, album_rows, active_column == 1);
                            draw_browser_column(config, song, start_row, song_rows, active_column == 2);
                        }
                        if (!status_base.empty())
                        {
                            draw_status_line(status_base + " | /" + search_query);
                        }
                    }
                }

                if (key == '\r' || key == '\n')
                {
                    browser_column* target = nullptr;
                    if (active_column == 0)
                    {
                        target = &artist;
                    }
                    else if (active_column == 1)
                    {
                        target = &album;
                    }
                    else
                    {
                        target = &song;
                    }

                    if (target != nullptr && target->selected_index >= 0)
                    {
                        const auto& selected = target->entries[target->selected_index];
                        if (selected.is_dir && active_column < 2)
                        {
                            if (active_column == 0)
                            {
                                album.path = selected.path;
                                refresh_column(album);
                                if (album.selected_index >= 0)
                                {
                                    const auto& album_selected = album.entries[album.selected_index];
                                    song.path = album_selected.is_dir ? album_selected.path : std::filesystem::path();
                                }
                                else
                                {
                                    song.path.clear();
                                }
                                refresh_column(song);
                            }
                            else if (active_column == 1)
                            {
                                song.path = selected.is_dir ? selected.path : std::filesystem::path();
                                refresh_column(song);
                            }
                            active_column = std::min(2, active_column + 1);
                            dirty = true;
                        }
                        else if (!selected.is_dir)
                        {
                            player.set_current_track(selected.path.string());
                            current_album = album.selected_index >= 0 ? album.entries[album.selected_index].name : "";
                            current_artist = artist.selected_index >= 0 ? artist.entries[artist.selected_index].name : "";
                            player.stop_playback();
                            playback_active = player.start_playback(player.get_current_track()) == 0;
                            playback_failed = !playback_active;
                            if (playback_failed)
                            {
                                draw_status_line("Playback failed");
                            }
                            album_art.begin_fetch(player.get_current_track(), config, current_artist, current_album);
                            if (!album_art.is_pending())
                            {
                                full_redraw();
                            }
                            else
                            {
                                draw_browsers(config, artist, album, song, active_column);
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
                        }
                    }
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

                    active_column = std::max(0, active_column - 1);
                    dirty = true;
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

                    active_column = std::min(2, active_column + 1);
                    dirty = true;
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

                    int direction = (key == input_key_up) ? -1 : 1;
                    browser_column* target = nullptr;
                    if (active_column == 0)
                    {
                        target = &artist;
                    }
                    else if (active_column == 1)
                    {
                        target = &album;
                    }
                    else
                    {
                        target = &song;
                    }

                    if (target != nullptr && !target->entries.empty())
                    {
                        target->selected_index = std::clamp(
                            target->selected_index + direction,
                            0,
                            static_cast<int>(target->entries.size() - 1));
                        dirty = true;
                    }

                    if (active_column == 0)
                    {
                        if (artist.selected_index >= 0)
                        {
                            const auto& selected = artist.entries[artist.selected_index];
                            album.path = selected.is_dir ? selected.path : std::filesystem::path();
                        }
                        else
                        {
                            album.path.clear();
                        }
                        refresh_column(album);

                        if (album.selected_index >= 0)
                        {
                            const auto& selected = album.entries[album.selected_index];
                            song.path = selected.is_dir ? selected.path : std::filesystem::path();
                        }
                        else
                        {
                            song.path.clear();
                        }
                        refresh_column(song);
                    }
                    else if (active_column == 1)
                    {
                        if (album.selected_index >= 0)
                        {
                            const auto& selected = album.entries[album.selected_index];
                            song.path = selected.is_dir ? selected.path : std::filesystem::path();
                        }
                        else
                        {
                            song.path.clear();
                        }
                        refresh_column(song);
                    }

                    if (update_layout(config, artist, album, song, box_gap, art_origin_x, metadata_origin_x))
                    {
                        full_redraw();
                    }
                }
            }

        if (album_art.consume_dirty())
        {
            full_redraw();
        }

            if (dirty)
            {
                if (update_layout(config, artist, album, song, box_gap, art_origin_x, metadata_origin_x))
                {
                    full_redraw();
                    dirty = false;
                }
                else
                {
                    TerminalSize size = get_terminal_size();
                    int start_row = 3;
                    int max_rows = std::max(0, size.rows - start_row + 1);
                    int artist_rows = std::min(max_rows, static_cast<int>(artist.entries.size()));
                    int album_rows = std::min(max_rows, static_cast<int>(album.entries.size()));
                    int song_rows = std::min(max_rows, static_cast<int>(song.entries.size()));
                    fill_browser_area(config, artist, start_row, std::max(prev_artist_rows, artist_rows));
                    fill_browser_area(config, album, start_row, std::max(prev_album_rows, album_rows));
                    fill_browser_area(config, song, start_row, std::max(prev_song_rows, song_rows));
                    draw_browsers(config, artist, album, song, active_column);
                    prev_artist_rows = artist_rows;
                    prev_album_rows = album_rows;
                    prev_song_rows = song_rows;
                    if (!status_base.empty())
                    {
                        if (search_active)
                        {
                            draw_status_line(status_base + " | /" + search_query);
                        }
                        else
                        {
                            draw_status_line(status_base);
                        }
                    }
                    dirty = false;
                }
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
