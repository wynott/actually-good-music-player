#include "art_render.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"

#include "art_render.h"
#include "album_art.h"
#include "config.h"
#include "http.h"
#include "terminal.h"
#include "draw.h"
#include "vendor/json.hpp"

bool load_mp3_embedded_art(const char* path, std::vector<unsigned char>& image_data);

static void append_color(std::string& out, int r, int g, int b)
{
    out.append("\x1b[38;2;");
    out.append(std::to_string(r));
    out.push_back(';');
    out.append(std::to_string(g));
    out.push_back(';');
    out.append(std::to_string(b));
    out.push_back('m');
}

static void draw_centered_message(int start_col, int start_row, int out_w, int out_h, const std::string& message)
{
    int msg_row = start_row - (out_h / 2);
    int msg_col = start_col + std::max(0, (out_w - static_cast<int>(message.size())) / 2);
    std::cout << "\x1b[0m\x1b[" << msg_row << ";" << msg_col << "H" << message;
    std::cout.flush();
}

static void draw_log_message(int start_col, int start_row, int out_w, int out_h, int line_index, const std::string& message)
{
    int base_row = start_row - (out_h / 2);
    int msg_row = base_row + line_index;
    int msg_col = start_col + std::max(0, (out_w - static_cast<int>(message.size())) / 2);
    std::cout << "\x1b[0m\x1b[" << msg_row << ";" << msg_col << "H" << message;
    std::cout.flush();
}

static bool fetch_album_art_online(const std::string& artist, const std::string& album, std::vector<unsigned char>& image_data, std::string& error)
{
    if (artist.empty() || album.empty())
    {
        error = "Missing artist/album";
        return false;
    }

    std::string query = "https://musicbrainz.org/ws/2/release/?query=artist:%22" +
        url_encode(artist) + "%22%20AND%20release:%22" + url_encode(album) + "%22&fmt=json&limit=1";

    std::vector<unsigned char> response;
    std::string user_agent = "ActuallyGoodMusicPlayer/0.1 (https://github.com/wynott/actually-good-music-player)";
    if (!http_get(query, user_agent, "application/json", response))
    {
        error = "MusicBrainz request failed";
        return false;
    }

    std::string json_text(response.begin(), response.end());
    try
    {
        auto json = nlohmann::json::parse(json_text);
        if (!json.contains("releases") || json["releases"].empty())
        {
            error = "No releases found";
            return false;
        }

        std::string mbid = json["releases"][0]["id"].get<std::string>();
        if (mbid.empty())
        {
            error = "Empty release id";
            return false;
        }

        std::string cover_url = "https://coverartarchive.org/release/" + mbid + "/front-250";
        image_data.clear();
        if (!http_get(cover_url, user_agent, "image/*", image_data))
        {
            error = "Cover Art Archive failed";
            return false;
        }
        return true;
    }
    catch (...)
    {
        error = "MusicBrainz parse failed";
        return false;
    }
}

art_result start_album_art_fetch(
    const char* path,
    const app_config& config,
    const std::string& artist,
    const std::string& album)
{
    art_result result;
    result.ready = false;
    result.has_art = false;
    result.online_failed = false;

    std::vector<unsigned char> art_data;
    bool has_art = load_mp3_embedded_art(path, art_data);
    if (has_art)
    {
        result.ready = true;
        result.has_art = true;
        result.data = std::move(art_data);
        return result;
    }

    if (!config.enable_online_art)
    {
        result.ready = true;
        result.error_message = "No embedded art";
    }

    return result;
}

void complete_album_art_fetch(
    art_result& result,
    const app_config& config,
    const std::string& artist,
    const std::string& album)
{
    if (result.ready || !config.enable_online_art)
    {
        return;
    }

    std::vector<unsigned char> data;
    std::string error;
    bool ok = fetch_album_art_online(artist, album, data, error);
    result.ready = true;
    result.has_art = ok;
    result.online_failed = !ok;
    result.data = std::move(data);
    result.error_message = error;
}

bool render_album_art(
    const art_result& result,
    const app_config& config,
    int origin_x,
    int origin_y,
    app_config::rgb_color* out_avg_color)
{
    TerminalSize size = get_terminal_size();
    int max_cols = size.columns;
    int max_rows = size.rows;
    if (max_cols <= 0 || max_rows <= 0)
    {
        return false;
    }

    max_cols = std::min(max_cols, config.art_width_chars);
    max_rows = std::min(max_rows, config.art_height_chars);

    int out_w = std::max(1, max_cols);
    int out_h = std::max(1, max_rows);
    if (out_w <= 0 || out_h <= 0)
    {
        return false;
    }

    int max_cols_bound = size.columns;
    int max_rows_bound = size.rows;
    int start_col = std::max(1, origin_x + 1);
    int start_row = std::max(1, max_rows_bound - origin_y);

    if (!result.ready)
    {
        Renderer::instance().clear_screen(glm::vec3(0.0f));
        Terminal::instance().scan_and_write_characters();
        draw_log_message(start_col, start_row, out_w, out_h, 0, "No embedded art");
        draw_log_message(start_col, start_row, out_w, out_h, 1, "Fetching online via MusicBrainz");
        if (out_avg_color)
        {
            out_avg_color->r = 0;
            out_avg_color->g = 0;
            out_avg_color->b = 0;
        }
        return true;
    }

    if (!result.has_art)
    {
        Renderer::instance().clear_screen(glm::vec3(0.0f));
        Terminal::instance().scan_and_write_characters();
        std::string message = result.error_message.empty() ? "Fetch failed" : result.error_message;
        draw_centered_message(start_col, start_row, out_w, out_h, message);
        if (out_avg_color)
        {
            out_avg_color->r = 0;
            out_avg_color->g = 0;
            out_avg_color->b = 0;
        }
        return true;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
        result.data.data(),
        static_cast<int>(result.data.size()),
        &width,
        &height,
        &channels,
        4);

    if (pixels == nullptr || width <= 0 || height <= 0)
    {
        Renderer::instance().clear_screen(glm::vec3(0.0f));
        Terminal::instance().scan_and_write_characters();
        draw_centered_message(start_col, start_row, out_w, out_h, "Art decode failed");
        return true;
    }

    long long sum_r = 0;
    long long sum_g = 0;
    long long sum_b = 0;
    long long count = 0;

    std::string output;
    output.reserve(static_cast<size_t>(out_w * out_h * 20));

    for (int y = 0; y < out_h; ++y)
    {
        int world_y = (out_h - 1) - y;
        int row = start_row - world_y;
        if (row < 1 || row > max_rows_bound)
        {
            continue;
        }

        output.append("\x1b[");
        output.append(std::to_string(row));
        output.push_back(';');
        output.append(std::to_string(start_col));
        output.push_back('H');

        int src_y = static_cast<int>((static_cast<float>(y) / static_cast<float>(out_h)) * height);
        if (src_y >= height)
        {
            src_y = height - 1;
        }

        for (int x = 0; x < out_w; ++x)
        {
            int col = start_col + x;
            if (col < 1 || col > max_cols_bound)
            {
                break;
            }

            int src_x = static_cast<int>((static_cast<float>(x) / static_cast<float>(out_w)) * width);
            if (src_x >= width)
            {
                src_x = width - 1;
            }

            size_t index = static_cast<size_t>((src_y * width + src_x) * 4);
            int r = pixels[index + 0];
            int g = pixels[index + 1];
            int b = pixels[index + 2];

            sum_r += r;
            sum_g += g;
            sum_b += b;
            count += 1;

            append_color(output, r, g, b);
            output.append("█");
        }
    }

    if (count > 0)
    {
        int avg_r = static_cast<int>(sum_r / count);
        int avg_g = static_cast<int>(sum_g / count);
        int avg_b = static_cast<int>(sum_b / count);
        Renderer::instance().clear_screen(glm::vec3(static_cast<float>(avg_r), static_cast<float>(avg_g), static_cast<float>(avg_b)));
        Terminal::instance().scan_and_write_characters();
        if (out_avg_color)
        {
            out_avg_color->r = avg_r;
            out_avg_color->g = avg_g;
            out_avg_color->b = avg_b;
        }
    }
    else
    {
        Renderer::instance().clear_screen(glm::vec3(0.0f));
        Terminal::instance().scan_and_write_characters();
        if (out_avg_color)
        {
            out_avg_color->r = 0;
            out_avg_color->g = 0;
            out_avg_color->b = 0;
        }
    }

    output.append("\x1b[0m");
    std::cout << output;
    std::cout.flush();

    stbi_image_free(pixels);
    return true;
}

bool ArtRenderer::render(
    const art_result& result,
    const app_config& config,
    int origin_x,
    int origin_y,
    app_config::rgb_color* out_avg_color)
{
    TerminalSize size = get_terminal_size();
    int max_cols = size.columns;
    int max_rows = size.rows;
    if (max_cols <= 0 || max_rows <= 0)
    {
        return false;
    }

    max_cols = std::min(max_cols, config.art_width_chars);
    max_rows = std::min(max_rows, config.art_height_chars);

    int out_w = std::max(1, max_cols);
    int out_h = std::max(1, max_rows);
    if (out_w <= 0 || out_h <= 0)
    {
        return false;
    }

    int top_left_y = size.rows - origin_y - out_h;
    glm::ivec2 location(origin_x, top_left_y);
    _album_art.set_location(location);
    _album_art.set_size(glm::ivec2(out_w, out_h));

    auto clear_and_set_avg = [&](int r, int g, int b)
    {
        Renderer::instance().clear_screen(glm::vec3(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b)));
        Terminal::instance().scan_and_write_characters();
        if (out_avg_color)
        {
            out_avg_color->r = r;
            out_avg_color->g = g;
            out_avg_color->b = b;
        }
    };

    if (!result.ready || !result.has_art)
    {
        clear_and_set_avg(0, 0, 0);
        return true;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
        result.data.data(),
        static_cast<int>(result.data.size()),
        &width,
        &height,
        &channels,
        4);

    if (pixels == nullptr || width <= 0 || height <= 0)
    {
        clear_and_set_avg(0, 0, 0);
        return true;
    }

    long long sum_r = 0;
    long long sum_g = 0;
    long long sum_b = 0;
    long long count = 0;

    for (int y = 0; y < out_h; ++y)
    {
        int src_y = static_cast<int>((static_cast<float>(y) / static_cast<float>(out_h)) * height);
        if (src_y >= height)
        {
            src_y = height - 1;
        }

        for (int x = 0; x < out_w; ++x)
        {
            int src_x = static_cast<int>((static_cast<float>(x) / static_cast<float>(out_w)) * width);
            if (src_x >= width)
            {
                src_x = width - 1;
            }

            size_t index = static_cast<size_t>((src_y * width + src_x) * 4);
            int r = pixels[index + 0];
            int g = pixels[index + 1];
            int b = pixels[index + 2];

            sum_r += r;
            sum_g += g;
            sum_b += b;
            count += 1;
        }
    }

    stbi_image_free(pixels);

    if (!_album_art.load(result.data))
    {
        clear_and_set_avg(0, 0, 0);
        return true;
    }

    _album_art.draw();

    glm::ivec2 min_corner = _album_art.get_location();
    glm::ivec2 max_corner = min_corner + _album_art.get_size() - glm::ivec2(1);
    Terminal::instance().write_region(min_corner, max_corner);

    if (count > 0)
    {
        int avg_r = static_cast<int>(sum_r / count);
        int avg_g = static_cast<int>(sum_g / count);
        int avg_b = static_cast<int>(sum_b / count);
        clear_and_set_avg(avg_r, avg_g, avg_b);
    }
    else
    {
        clear_and_set_avg(0, 0, 0);
    }

    return true;
}
