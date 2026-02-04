#include <iostream>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <exception>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "player.h"
#include "art_render.h"
#include "config.h"
#include "input.h"
#include "net.h"
#include "state.h"
#include "terminal.h"
#include "metadata.h"
#include "http.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#if defined(_WIN32)
#include <windows.h>
#endif

static void draw_frame()
{
    TerminalSize size = get_terminal_size();
    if (size.columns <= 0 || size.rows <= 0)
    {
        return;
    }

    int columns = size.columns;
    int rows = size.rows;
    int right_col = (columns > 1) ? columns - 1 : 1;
    int bottom_row = (rows > 1) ? rows - 1 : 1;

    auto lerp = [](float a, float b, float t)
    {
        return a + (b - a) * t;
    };

    auto clamp_byte = [](float value)
    {
        if (value < 0.0f)
        {
            value = 0.0f;
        }
        if (value > 255.0f)
        {
            value = 255.0f;
        }
        return static_cast<int>(value + 0.5f);
    };

    std::string output;
    output.reserve(static_cast<size_t>(columns * rows * 20));
    output.append("\x1b[2J\x1b[H");

    for (int y = 0; y < rows; ++y)
    {
        float v = (rows > 1) ? static_cast<float>(y) / static_cast<float>(rows - 1) : 0.0f;
        for (int x = 0; x < columns; ++x)
        {
            float u = (columns > 1) ? static_cast<float>(x) / static_cast<float>(columns - 1) : 0.0f;

            float tl_r = 255.0f;
            float tl_g = 0.0f;
            float tl_b = 0.0f;

            float tr_r = 0.0f;
            float tr_g = 255.0f;
            float tr_b = 0.0f;

            float bl_r = 0.0f;
            float bl_g = 0.0f;
            float bl_b = 255.0f;

            float br_r = 255.0f;
            float br_g = 255.0f;
            float br_b = 255.0f;

            float top_r = lerp(tl_r, tr_r, u);
            float top_g = lerp(tl_g, tr_g, u);
            float top_b = lerp(tl_b, tr_b, u);

            float bottom_r = lerp(bl_r, br_r, u);
            float bottom_g = lerp(bl_g, br_g, u);
            float bottom_b = lerp(bl_b, br_b, u);

            int r = clamp_byte(lerp(top_r, bottom_r, v));
            int g = clamp_byte(lerp(top_g, bottom_g, v));
            int b = clamp_byte(lerp(top_b, bottom_b, v));

            output.append("\x1b[38;2;");
            output.append(std::to_string(r));
            output.push_back(';');
            output.append(std::to_string(g));
            output.push_back(';');
            output.append(std::to_string(b));
            output.append("m█");
        }
        if (y + 1 < rows)
        {
            output.push_back('\n');
        }
    }

    output.append("\x1b[0m");
    output.append("\x1b[1;1H\x1b[31mr\x1b[0m");
    output.append("\x1b[1;");
    output.append(std::to_string(right_col));
    output.append("H\x1b[32mg\x1b[0m");
    output.append("\x1b[");
    output.append(std::to_string(bottom_row));
    output.append(";1H\x1b[34mb\x1b[0m");
    output.append("\x1b[");
    output.append(std::to_string(bottom_row));
    output.append(";");
    output.append(std::to_string(right_col));
    output.append("H0");
    output.append("\x1b[2;2HHello, world!");

    std::cout << output;
    std::cout.flush();
}

struct browser_column
{
    std::filesystem::path path;
    struct entry
    {
        std::string name;
        std::filesystem::path path;
        bool is_dir;
    };

    std::vector<entry> entries;
    int selected_index;
    int width;
    int start_col;
};

static bool update_layout(
    const app_config& config,
    browser_column& artist,
    browser_column& album,
    browser_column& song,
    int box_gap,
    int& art_origin_x,
    int& metadata_origin_x)
{
    int prev_art_origin = art_origin_x;

    artist.width = std::max(1, config.col_width_artist);
    album.width = std::max(1, config.col_width_album);
    song.width = std::max(1, config.col_width_song);

    int artist_box_start = 1;
    int artist_box_width = artist.width + 2;
    int album_box_start = artist_box_start + artist_box_width + box_gap;
    int album_box_width = album.width + 2;
    int song_box_start = album_box_start + album_box_width + box_gap;
    int song_box_width = song.width + 2;

    artist.start_col = artist_box_start + 1;
    album.start_col = album_box_start + 1;
    song.start_col = song_box_start + 1;

    int browser_width = artist_box_width + album_box_width + song_box_width + box_gap * 2;
    art_origin_x = std::max(0, browser_width + box_gap);
    metadata_origin_x = art_origin_x + config.art_width_chars + box_gap;

    return prev_art_origin != art_origin_x;
}

static void draw_browser_row(const app_config& config, const browser_column& column, int row_index, int start_row, bool active);

static std::vector<browser_column::entry> list_entries(const std::filesystem::path& path)
{
    namespace fs = std::filesystem;
    std::vector<browser_column::entry> result;
    try
    {
        if (fs::exists(path) && fs::is_directory(path))
        {
            for (const auto& entry : fs::directory_iterator(path))
            {
                browser_column::entry item;
                item.name = entry.path().filename().string();
                item.path = entry.path();
                item.is_dir = entry.is_directory();
                result.push_back(item);
            }
        }
    }
    catch (...)
    {
    }

    std::sort(result.begin(), result.end(), [](const browser_column::entry& a, const browser_column::entry& b)
    {
        return a.name < b.name;
    });
    return result;
}

static void refresh_column(browser_column& column)
{
    column.entries = list_entries(column.path);
    if (column.entries.empty())
    {
        column.selected_index = -1;
    }
    else
    {
        column.selected_index = std::clamp(column.selected_index, 0, static_cast<int>(column.entries.size() - 1));
    }
}

static void draw_browser_column(const app_config& config, const browser_column& column, int start_row, int max_rows, bool active)
{
    if (max_rows <= 0)
    {
        return;
    }

    for (int i = 0; i < max_rows; ++i)
    {
        draw_browser_row(config, column, i, start_row, active);
    }

    std::cout.flush();
}

static void draw_browser_row(const app_config& config, const browser_column& column, int row_index, int start_row, bool active)
{
    if (row_index < 0)
    {
        return;
    }

    int row = start_row + row_index;
    std::cout << "\x1b[" << row << ";" << column.start_col << "H";

    auto fg = [&](const app_config::rgb_color& color)
    {
        std::cout << "\x1b[38;2;" << color.r << ";" << color.g << ";" << color.b << "m";
    };
    auto bg = [&](const app_config::rgb_color& color)
    {
        std::cout << "\x1b[48;2;" << color.r << ";" << color.g << ";" << color.b << "m";
    };

    bool is_selected = (column.selected_index == row_index);
    if (is_selected)
    {
        if (active)
        {
            fg(config.browser_selected_fg);
            bg(config.browser_selected_bg);
        }
        else
        {
            fg(config.browser_inactive_fg);
            bg(config.browser_inactive_bg);
        }
    }
    else
    {
        fg(config.browser_normal_fg);
        bg(config.browser_normal_bg);
    }

    std::string line;
    if (row_index < static_cast<int>(column.entries.size()))
    {
        line = column.entries[row_index].name;
    }

    if (static_cast<int>(line.size()) > column.width)
    {
        line.resize(static_cast<size_t>(column.width));
    }
    if (static_cast<int>(line.size()) < column.width)
    {
        line.append(static_cast<size_t>(column.width - line.size()), ' ');
    }

    std::cout << line;
    std::cout << "\x1b[0m";
    std::cout.flush();
}

static void draw_browser_box(const app_config& config, const browser_column& column, int start_row, int max_rows)
{
    if (max_rows <= 0)
    {
        return;
    }
    int top_row = start_row - 1;
    int bottom_row = start_row + max_rows;
    int box_start_col = column.start_col - 1;
    int box_width = column.width + 2;

    if (top_row < 1 || box_start_col < 1)
    {
        return;
    }

    std::string horizontal;
    horizontal.reserve(static_cast<size_t>(column.width) * 3);
    for (int i = 0; i < column.width; ++i)
    {
        horizontal.append("─");
    }

    std::string top = std::string("╭") + horizontal + "╮";
    std::string bottom = std::string("╰") + horizontal + "╯";

    std::cout << "\x1b[38;2;" << config.browser_normal_fg.r << ";" << config.browser_normal_fg.g << ";" << config.browser_normal_fg.b << "m";
    std::cout << "\x1b[48;2;" << config.browser_normal_bg.r << ";" << config.browser_normal_bg.g << ";" << config.browser_normal_bg.b << "m";

    std::cout << "\x1b[" << top_row << ";" << box_start_col << "H" << top;
    for (int r = start_row; r < bottom_row; ++r)
    {
        std::cout << "\x1b[" << r << ";" << box_start_col << "H" << "│";
        std::cout << "\x1b[" << r << ";" << (box_start_col + box_width - 1) << "H" << "│";
    }
    std::cout << "\x1b[" << bottom_row << ";" << box_start_col << "H" << bottom;
    std::cout << "\x1b[0m";
    std::cout.flush();
}

static void draw_browsers(const app_config& config, browser_column& artist, browser_column& album, browser_column& song, int active_column)
{
    TerminalSize size = get_terminal_size();
    if (size.columns <= 0 || size.rows <= 0)
    {
        return;
    }

    int start_row = 3;
    int max_rows = std::max(0, size.rows - start_row + 1);

    int artist_rows = std::min(max_rows, static_cast<int>(artist.entries.size()));
    int album_rows = std::min(max_rows, static_cast<int>(album.entries.size()));
    int song_rows = std::min(max_rows, static_cast<int>(song.entries.size()));

    draw_browser_box(config, artist, start_row, artist_rows);
    draw_browser_box(config, album, start_row, album_rows);
    draw_browser_box(config, song, start_row, song_rows);

    draw_browser_column(config, artist, start_row, artist_rows, active_column == 0);
    draw_browser_column(config, album, start_row, album_rows, active_column == 1);
    draw_browser_column(config, song, start_row, song_rows, active_column == 2);
}

static void fill_browser_area(const app_config& config, const browser_column& column, int start_row, int rows)
{
    if (rows <= 0)
    {
        return;
    }

    int box_start_col = column.start_col - 1;
    int box_width = column.width + 2;
    std::string line(static_cast<size_t>(box_width), ' ');
    std::cout << "\x1b[38;2;" << config.browser_normal_bg.r << ";" << config.browser_normal_bg.g << ";" << config.browser_normal_bg.b << "m";
    std::cout << "\x1b[48;2;" << config.browser_normal_bg.r << ";" << config.browser_normal_bg.g << ";" << config.browser_normal_bg.b << "m";

    for (int i = 0; i < rows + 2; ++i)
    {
        int row = start_row - 1 + i;
        std::cout << "\x1b[" << row << ";" << box_start_col << "H" << line;
    }

    std::cout << "\x1b[0m";
}

static void draw_status_line(const std::string& text)
{
    std::cout << "\x1b[1;2H" << text << "\x1b[0m";
    std::cout.flush();
}

static void draw_metadata_panel(const app_config& config, const track_metadata& meta, int start_col, int start_row)
{
    if (start_col < 1 || start_row < 1)
    {
        return;
    }

    std::vector<std::string> lines;
    if (!meta.title.empty()) lines.push_back("Title: " + meta.title);
    if (!meta.artist.empty()) lines.push_back("Artist: " + meta.artist);
    if (!meta.album.empty()) lines.push_back("Album: " + meta.album);
    if (!meta.date.empty()) lines.push_back("Date: " + meta.date);
    if (!meta.genre.empty()) lines.push_back("Genre: " + meta.genre);
    if (!meta.track.empty()) lines.push_back("Track: " + meta.track);
    if (meta.sample_rate > 0) lines.push_back("Hz: " + std::to_string(meta.sample_rate));
    if (meta.channels > 0) lines.push_back("Channels: " + std::to_string(meta.channels));
    if (meta.duration_ms > 0) lines.push_back("Length: " + std::to_string(meta.duration_ms / 1000) + "s");
    if (meta.bitrate_kbps > 0) lines.push_back("Bitrate: " + std::to_string(meta.bitrate_kbps) + " kbps");
    if (meta.file_size_bytes > 0) lines.push_back("Size: " + std::to_string(meta.file_size_bytes / 1024) + " KB");

    int row = start_row;
    for (auto line : lines)
    {
        if (config.metadata_max_width > 0 && static_cast<int>(line.size()) > config.metadata_max_width)
        {
            line.resize(static_cast<size_t>(config.metadata_max_width));
        }
        std::cout << "\x1b[" << row << ";" << start_col << "H" << line;
        row += 1;
    }
    std::cout.flush();
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

int main()
{
    try
    {
        auto log = spdlog::basic_logger_mt("agmp", "agmp.log", true);
        spdlog::set_default_logger(log);
        spdlog::set_level(spdlog::level::info);
        spdlog::set_pattern("[%H:%M:%S] [%l] %v");
        spdlog::flush_on(spdlog::level::info);

        http_init();
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

    app_config config = load_config("config.toml");
    spdlog::info("config: loaded");
    const char* path = config.default_track.c_str();
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
    spdlog::info("network: started={}, port={}", network_started, config.listen_port);
    std::string status_base;
    if (network_started)
    {
        status_base = "IP: " + info.ip_address + ":" + std::to_string(info.port);
        draw_status_line(status_base);
    }

    std::string current_track = path;
    app_state state = load_state("state.toml");
    if (config.auto_resume_playback && !state.last_track.empty())
    {
        current_track = state.last_track;
    }

    std::string current_artist;
    std::string current_album;
    try
    {
        std::filesystem::path track_path(current_track);
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
    art_result art_state;
    std::mutex art_mutex;
    std::atomic<bool> art_dirty(false);
    std::thread art_thread;

    bool art_pending = false;

    auto begin_art_fetch = [&]()
    {
        if (art_thread.joinable())
        {
            art_thread.join();
        }

        art_state = start_album_art_fetch(current_track.c_str(), config, current_artist, current_album);
        art_pending = !art_state.ready;
        if (art_state.ready)
        {
            art_dirty.store(true, std::memory_order_release);
        }

        if (!art_state.ready && config.enable_online_art)
        {
            std::string artist_copy = current_artist;
            std::string album_copy = current_album;
            art_thread = std::thread([&]()
            {
                try
                {
                    art_result local = art_state;
                    complete_album_art_fetch(local, config, artist_copy, album_copy);
                    {
                        std::lock_guard<std::mutex> lock(art_mutex);
                        art_state = std::move(local);
                    }
                    art_dirty.store(true, std::memory_order_release);
                }
                catch (const std::exception& ex)
                {
                    spdlog::error("art thread error: {}", ex.what());
                }
                catch (...)
                {
                    spdlog::error("art thread unknown error");
                }
            });
        }
    };

    auto full_redraw = [&]()
    {
        update_layout(config, artist, album, song, box_gap, art_origin_x, metadata_origin_x);
        app_config::rgb_color avg_color = config.browser_normal_bg;
        art_result snapshot;
        {
            std::lock_guard<std::mutex> lock(art_mutex);
            snapshot = art_state;
        }
        bool rendered = render_album_art(
            snapshot,
            config,
            art_origin_x,
            0,
            &avg_color);
        if (!rendered)
        {
            draw_frame();
        }
        config.browser_normal_bg = avg_color;
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
        track_metadata meta;
        if (read_track_metadata(current_track, meta))
        {
            draw_metadata_panel(config, meta, metadata_origin_x + 1, 3);
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

    begin_art_fetch();
    spdlog::info("art: pending={}", art_pending);
    if (!art_pending)
    {
        full_redraw();
    }
    else
    {
        draw_browsers(config, artist, album, song, active_column);
        spdlog::info("ui: browsers drawn (pending art)");
        track_metadata meta;
        if (read_track_metadata(current_track, meta))
        {
            draw_metadata_panel(config, meta, metadata_origin_x + 1, 3);
            spdlog::info("ui: metadata drawn");
        }
        if (!status_base.empty())
        {
            draw_status_line(status_base);
            spdlog::info("ui: status line drawn");
        }
    }

    track_metadata initial_meta;
    if (read_track_metadata(current_track, initial_meta) && initial_meta.duration_ms > 0)
    {
        if (state.last_position_ms >= initial_meta.duration_ms)
        {
            state.last_position_ms = 0;
        }
    }

    auto start_playback_or_fallback = [&]() -> bool
    {
        spdlog::info("playback: start attempt {}", current_track);
        if (agmp_start_playback(current_track.c_str()) == 0)
        {
            spdlog::info("playback: started");
            return true;
        }

        if (current_track != path)
        {
            current_track = path;
            current_artist.clear();
            current_album.clear();
            try
            {
                std::filesystem::path track_path(current_track);
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

            return agmp_start_playback(current_track.c_str()) == 0;
        }

        spdlog::info("playback: failed");
        return false;
    };

    bool playback_active = start_playback_or_fallback();
    bool playback_failed = !playback_active;
    spdlog::info("playback: start={}, failed={}", playback_active, playback_failed);
    if (config.auto_resume_playback && playback_active && state.last_position_ms > 0 && current_track == state.last_track)
    {
        agmp_seek_ms(state.last_position_ms);
    }
    if (playback_failed)
    {
        draw_status_line("Playback failed");
    }

    input_init();
    bool quit = false;
    bool dirty = false;
    spdlog::info("loop: enter");
    while (!quit)
    {
        // trace removed
        if (playback_active && agmp_is_done())
        {
            if (song.selected_index >= 0 && song.selected_index + 1 < static_cast<int>(song.entries.size()))
            {
                song.selected_index += 1;
                current_track = song.entries[song.selected_index].path.string();
                current_album = album.selected_index >= 0 ? album.entries[album.selected_index].name : "";
                current_artist = artist.selected_index >= 0 ? artist.entries[artist.selected_index].name : "";
                agmp_stop_playback();
                playback_active = agmp_start_playback(current_track.c_str()) == 0;
                playback_failed = !playback_active;
                if (playback_failed)
                {
                    draw_status_line("Playback failed");
                }
                begin_art_fetch();
                full_redraw();
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        int raw_key = input_poll_key();
        if (raw_key != -1)
        {
            spdlog::info("input: raw_key={}", raw_key);
        }
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
                agmp_toggle_pause();
                spdlog::info("input: pause toggled");
            }
            if (ch == quit_key)
            {
                quit = true;
                spdlog::info("input: quit requested");
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
                            if (art_thread.joinable())
                            {
                                art_thread.join();
                            }
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
                        current_track = selected.path.string();
                        current_album = album.selected_index >= 0 ? album.entries[album.selected_index].name : "";
                        current_artist = artist.selected_index >= 0 ? artist.entries[artist.selected_index].name : "";
                        agmp_stop_playback();
                        playback_active = agmp_start_playback(current_track.c_str()) == 0;
                        playback_failed = !playback_active;
                        if (playback_failed)
                        {
                            draw_status_line("Playback failed");
                        }
                        begin_art_fetch();
                        if (!art_pending)
                        {
                            full_redraw();
                        }
                        else
                        {
                            draw_browsers(config, artist, album, song, active_column);
                            track_metadata meta;
                            if (read_track_metadata(current_track, meta))
                            {
                                draw_metadata_panel(config, meta, metadata_origin_x + 1, 3);
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
                active_column = std::max(0, active_column - 1);
                dirty = true;
            }
            if (key == input_key_right)
            {
                active_column = std::min(2, active_column + 1);
                dirty = true;
            }
            if (key == input_key_up || key == input_key_down)
            {
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

        if (art_dirty.exchange(false, std::memory_order_acq_rel))
        {
            art_pending = false;
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

    spdlog::info("main: shutdown");
    input_shutdown();
    http_cleanup();
    if (art_thread.joinable())
    {
        art_thread.join();
    }
    app_state save;
    save.last_track = current_track;
    save.last_position_ms = agmp_get_position_ms();
    save_state("state.toml", save);
    agmp_stop_playback();
    stop_network();
    std::cout << "\x1b[2J\x1b[H\x1b[0m";
    std::cout.flush();
    return 0;
    }
    catch (const std::exception& ex)
    {
        std::cout << "Fatal error: " << ex.what() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 1;
    }
    catch (...)
    {
        std::cout << "Fatal error: unknown" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 1;
    }
}
