#include "album_art.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "album_art.h"
#include "spdlog/spdlog.h"
#include "terminal.h"

#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb/stb_image.h"

#include "draw.h"
#include "http.h"
#include "player.h"
#include "vendor/json/single_include/nlohmann/json.hpp"


static uint32_t read_be32(const uint8_t* data)
{
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

static uint32_t read_syncsafe32(const uint8_t* data)
{
    return (static_cast<uint32_t>(data[0]) << 21) |
           (static_cast<uint32_t>(data[1]) << 14) |
           (static_cast<uint32_t>(data[2]) << 7) |
           static_cast<uint32_t>(data[3]);
}

static size_t skip_text(const uint8_t* data, size_t size, uint8_t encoding)
{
    if (encoding == 0 || encoding == 3)
    {
        for (size_t i = 0; i < size; ++i)
        {
            if (data[i] == 0)
            {
                return i + 1;
            }
        }
        return size;
    }

    for (size_t i = 0; i + 1 < size; i += 2)
    {
        if (data[i] == 0 && data[i + 1] == 0)
        {
            return i + 2;
        }
    }
    return size;
}

static bool extract_apic(const uint8_t* frame_data, size_t frame_size, std::vector<unsigned char>& image_data)
{
    if (frame_size < 4)
    {
        return false;
    }

    uint8_t encoding = frame_data[0];
    size_t offset = 1;

    size_t mime_end = offset;
    while (mime_end < frame_size && frame_data[mime_end] != 0)
    {
        ++mime_end;
    }
    if (mime_end >= frame_size)
    {
        return false;
    }

    offset = mime_end + 1;
    if (offset >= frame_size)
    {
        return false;
    }

    offset += 1;
    if (offset >= frame_size)
    {
        return false;
    }

    size_t desc_skip = skip_text(frame_data + offset, frame_size - offset, encoding);
    offset += desc_skip;
    if (offset >= frame_size)
    {
        return false;
    }

    image_data.assign(frame_data + offset, frame_data + frame_size);
    return !image_data.empty();
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

static art_result start_album_art_fetch(
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

static void complete_album_art_fetch(
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

bool load_mp3_embedded_art(const char* path, std::vector<unsigned char>& image_data)
{
    image_data.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return false;
    }

    file.seekg(0, std::ios::end);
    std::streamoff file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    uint8_t header[10] = {};
    file.read(reinterpret_cast<char*>(header), sizeof(header));
    if (file.gcount() != static_cast<std::streamsize>(sizeof(header)))
    {
        return false;
    }

    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3')
    {
        return false;
    }

    uint8_t version_major = header[3];
    uint8_t flags = header[5];
    uint32_t tag_size = read_syncsafe32(&header[6]);
    const uint32_t max_tag_size = 16 * 1024 * 1024;
    if (tag_size == 0 || tag_size > max_tag_size)
    {
        return false;
    }
    if (file_size > 0 && static_cast<std::streamoff>(tag_size) > file_size - 10)
    {
        return false;
    }

    std::vector<uint8_t> tag_data(tag_size);
    file.read(reinterpret_cast<char*>(tag_data.data()), static_cast<std::streamsize>(tag_data.size()));
    if (file.gcount() != static_cast<std::streamsize>(tag_data.size()))
    {
        return false;
    }

    size_t offset = 0;
    if ((flags & 0x40) != 0 && version_major >= 3)
    {
        if (tag_data.size() < 4)
        {
            return false;
        }

        uint32_t ext_size = (version_major == 4)
            ? read_syncsafe32(tag_data.data())
            : read_be32(tag_data.data());
        offset += static_cast<size_t>(ext_size) + 4;
        if (offset >= tag_data.size())
        {
            return false;
        }
    }

    if (version_major < 3)
    {
        return false;
    }

    while (offset + 10 <= tag_data.size())
    {
        const uint8_t* frame = tag_data.data() + offset;
        if (frame[0] == 0 && frame[1] == 0 && frame[2] == 0 && frame[3] == 0)
        {
            break;
        }

        uint32_t frame_size = (version_major == 4)
            ? read_syncsafe32(frame + 4)
            : read_be32(frame + 4);
        if (frame_size == 0)
        {
            break;
        }

        if (frame[0] == 'A' && frame[1] == 'P' && frame[2] == 'I' && frame[3] == 'C')
        {
            size_t data_offset = offset + 10;
            if (data_offset + frame_size <= tag_data.size())
            {
                return extract_apic(tag_data.data() + data_offset, frame_size, image_data);
            }
            return false;
        }

        size_t next = offset + 10 + static_cast<size_t>(frame_size);
        if (next <= offset || next > tag_data.size())
        {
            break;
        }
        offset = next;
    }

    return false;
}

AlbumArt::AlbumArt() = default;

AlbumArt::AlbumArt(const glm::ivec2& location, const glm::ivec2& size)
    : _location(location),
      _size(size)
{
}

AlbumArt::~AlbumArt()
{
    wait_for_fetch();
}

void AlbumArt::set_location(const glm::ivec2& location)
{
    _location = location;
}

void AlbumArt::set_size(const glm::ivec2& size)
{
    _size = size;
}

const glm::ivec2& AlbumArt::get_location() const
{
    return _location;
}

const glm::ivec2& AlbumArt::get_size() const
{
    return _size;
}

bool AlbumArt::load(const std::vector<unsigned char>& image_data)
{
    return load_image_data(image_data);
}

void AlbumArt::set_track(
    const std::string& path,
    const app_config& config,
    const std::string& artist,
    const std::string& album)
{
    if (_current_track == path && _current_artist == artist && _current_album == album)
    {
        return;
    }

    _current_track = path;
    _current_artist = artist;
    _current_album = album;

    if (config.safe_mode)
    {
        _pending.store(false, std::memory_order_release);
        return;
    }

    wait_for_fetch();

    art_result result = start_album_art_fetch(path.c_str(), config, artist, album);
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _result = std::move(result);
    }

    bool pending = !_result.ready;
    _pending.store(pending, std::memory_order_release);
    if (!pending)
    {
        _dirty.store(true, std::memory_order_release);
    }

    if (!_result.ready && config.enable_online_art)
    {
        std::string artist_copy = artist;
        std::string album_copy = album;
        _thread = std::thread([this, config, artist_copy, album_copy]()
        {
            try
            {
                art_result local;
                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    local = _result;
                }
                complete_album_art_fetch(local, config, artist_copy, album_copy);
                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    _result = std::move(local);
                }
                _dirty.store(true, std::memory_order_release);
            }
            catch (...)
            {
            }
            _pending.store(false, std::memory_order_release);
        });
    }
}

void AlbumArt::update_from_player(Player* player,
                                  const app_config& config,
                                  int origin_x,
                                  int origin_y)
{
    

    player->ensure_context_from_track();
    auto context = player->get_context();

    set_track(
        player->get_current_track(),
        config,
        context.artist,
        context.album);

    if (_dirty.exchange(false, std::memory_order_acq_rel))
    {
        render_current(config, origin_x, origin_y, nullptr);
    }
}

bool AlbumArt::render_current(
    const app_config& config,
    int origin_x,
    int origin_y,
    app_config::rgb_color* out_avg_color)
{
    art_result snapshot;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        snapshot = _result;
    }

    auto renderer = Renderer::get();
    if (!renderer)
    {
        return false;
    }
    auto size = renderer->get_terminal_size();

    int max_cols = size.x;
    int max_rows = size.y;
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

    int top_left_y = size.y - origin_y - out_h;
    set_location(glm::ivec2(origin_x, top_left_y));
    set_size(glm::ivec2(out_w, out_h));

    auto clear_and_set_avg = [&](const glm::vec4& colour)
    {
        if (out_avg_color)
        {
            out_avg_color->r = colour.r;
            out_avg_color->g = colour.g;
            out_avg_color->b = colour.b;
        }
    };

    if (!snapshot.ready || !snapshot.has_art)
    {
        clear_and_set_avg(glm::vec4(0.0f));

        renderer->set_canvas(
            glm::vec4(config.rice_background_tl.r, config.rice_background_tl.g, config.rice_background_tl.b, 1.0f),
            glm::vec4(config.rice_background_tr.r, config.rice_background_tr.g, config.rice_background_tr.b, 1.0f),
            glm::vec4(config.rice_background_bl.r, config.rice_background_bl.g, config.rice_background_bl.b, 1.0f),
            glm::vec4(config.rice_background_br.r, config.rice_background_br.g, config.rice_background_br.b, 1.0f));

        return true;
    }

    if (!load(snapshot.data))
    {
        clear_and_set_avg(glm::vec4(0.0f));

        renderer->set_canvas(
            glm::vec4(config.rice_background_tl.r, config.rice_background_tl.g, config.rice_background_tl.b, 1.0f),
            glm::vec4(config.rice_background_tr.r, config.rice_background_tr.g, config.rice_background_tr.b, 1.0f),
            glm::vec4(config.rice_background_bl.r, config.rice_background_bl.g, config.rice_background_bl.b, 1.0f),
            glm::vec4(config.rice_background_br.r, config.rice_background_br.g, config.rice_background_br.b, 1.0f));

        return true;
    }

    glm::vec4 avg_tl(0.0f);
    glm::vec4 avg_tr(0.0f);
    glm::vec4 avg_bl(0.0f);
    glm::vec4 avg_br(0.0f);
    average_colour(avg_tl, avg_tr, avg_bl, avg_br);
    clear_and_set_avg((avg_tl + avg_tr + avg_bl + avg_br) * 0.25f);
    renderer->set_canvas(avg_tl, avg_tr, avg_bl, avg_br);

    draw();
    return true;
}

void AlbumArt::wait_for_fetch()
{
    if (_thread.joinable())
    {
        _thread.join();
    }
}

bool AlbumArt::load_image_data(const std::vector<unsigned char>& image_data)
{
    if (image_data.empty())
    {
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
        image_data.data(),
        static_cast<int>(image_data.size()),
        &width,
        &height,
        &channels,
        4);

    if (pixels == nullptr || width <= 0 || height <= 0)
    {
        return false;
    }

    if (_size.x <= 0 || _size.y <= 0)
    {
        _size = glm::ivec2(width, height);
    }

    int out_w = std::max(1, _size.x);
    int out_h = std::max(1, _size.y);
    _pixels.assign(static_cast<size_t>(out_w * out_h), Terminal::Character{});

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

            size_t src_index = static_cast<size_t>((src_y * width + src_x) * 4);
            int r = pixels[src_index + 0];
            int g = pixels[src_index + 1];
            int b = pixels[src_index + 2];
            float inv = 1.0f / 255.0f;

            size_t dst_index = static_cast<size_t>(y * out_w + x);
            Terminal::Character& cell = _pixels[dst_index];
            cell.set_glyph(U'█');
            glm::vec4 colour(r * inv, g * inv, b * inv, 1.0f);
            cell.set_glyph_colour(colour);
            cell.set_background_colour(colour);
        }
    }

    stbi_image_free(pixels);
    return true;
}

void AlbumArt::draw() const
{
    spdlog::trace("AlbumArt::draw()");

    if (_pixels.empty() || _size.x <= 0 || _size.y <= 0)
    {
        return;
    }

    Renderer* renderer = Renderer::get();
    if (!renderer)
    {
        return;
    }
    glm::ivec2 terminal_size = renderer->get_terminal_size();

    if (terminal_size.x <= 0 || terminal_size.y <= 0)
    {
        return;
    }

    int start_x = std::max(0, _location.x);
    int start_y = std::max(0, _location.y);
    int end_x = std::min(_location.x + _size.x, terminal_size.x);
    int end_y = std::min(_location.y + _size.y, terminal_size.y);

    if (start_x >= end_x || start_y >= end_y)
    {
        return;
    }

    for (int y = start_y; y < end_y; ++y)
    {
        int src_y = y - _location.y;
        size_t row_offset = static_cast<size_t>(src_y * _size.x);
        for (int x = start_x; x < end_x; ++x)
        {
            int src_x = x - _location.x;
            size_t src_index = row_offset + static_cast<size_t>(src_x);
            if (src_index >= _pixels.size())
            {
                continue;
            }

            const Terminal::Character& src = _pixels[src_index];
            renderer->draw_glyph(
                glm::ivec2(x, y),
                src.get_glyph(),
                src.get_glyph_colour(),
                src.get_background_colour());
        }
    }
}

void AlbumArt::average_colour(glm::vec4& top_left, glm::vec4& top_right, glm::vec4& bottom_left, glm::vec4& bottom_right) const
{
    top_left = glm::vec4(0.0f);
    top_right = glm::vec4(0.0f);
    bottom_left = glm::vec4(0.0f);
    bottom_right = glm::vec4(0.0f);

    if (_pixels.empty() || _size.x <= 0 || _size.y <= 0)
    {
        return;
    }

    int mid_x = _size.x / 2;
    int mid_y = _size.y / 2;

    double sum_tl_r = 0.0;
    double sum_tl_g = 0.0;
    double sum_tl_b = 0.0;
    double sum_tr_r = 0.0;
    double sum_tr_g = 0.0;
    double sum_tr_b = 0.0;
    double sum_bl_r = 0.0;
    double sum_bl_g = 0.0;
    double sum_bl_b = 0.0;
    double sum_br_r = 0.0;
    double sum_br_g = 0.0;
    double sum_br_b = 0.0;

    double count_tl = 0.0;
    double count_tr = 0.0;
    double count_bl = 0.0;
    double count_br = 0.0;

    for (int y = 0; y < _size.y; ++y)
    {
        for (int x = 0; x < _size.x; ++x)
        {
            size_t index = static_cast<size_t>(y * _size.x + x);
            if (index >= _pixels.size())
            {
                continue;
            }

            glm::vec4 colour = _pixels[index].get_glyph_colour();
            bool left = x < mid_x;
            bool top = y < mid_y;

            if (top && left)
            {
                sum_tl_r += colour.r;
                sum_tl_g += colour.g;
                sum_tl_b += colour.b;
                count_tl += 1.0;
            }
            else if (top && !left)
            {
                sum_tr_r += colour.r;
                sum_tr_g += colour.g;
                sum_tr_b += colour.b;
                count_tr += 1.0;
            }
            else if (!top && left)
            {
                sum_bl_r += colour.r;
                sum_bl_g += colour.g;
                sum_bl_b += colour.b;
                count_bl += 1.0;
            }
            else
            {
                sum_br_r += colour.r;
                sum_br_g += colour.g;
                sum_br_b += colour.b;
                count_br += 1.0;
            }
        }
    }

    if (count_tl > 0.0)
    {
        top_left = glm::vec4(
            static_cast<float>(sum_tl_r / count_tl),
            static_cast<float>(sum_tl_g / count_tl),
            static_cast<float>(sum_tl_b / count_tl),
            1.0f);
    }
    if (count_tr > 0.0)
    {
        top_right = glm::vec4(
            static_cast<float>(sum_tr_r / count_tr),
            static_cast<float>(sum_tr_g / count_tr),
            static_cast<float>(sum_tr_b / count_tr),
            1.0f);
    }
    if (count_bl > 0.0)
    {
        bottom_left = glm::vec4(
            static_cast<float>(sum_bl_r / count_bl),
            static_cast<float>(sum_bl_g / count_bl),
            static_cast<float>(sum_bl_b / count_bl),
            1.0f);
    }
    if (count_br > 0.0)
    {
        bottom_right = glm::vec4(
            static_cast<float>(sum_br_r / count_br),
            static_cast<float>(sum_br_g / count_br),
            static_cast<float>(sum_br_b / count_br),
            1.0f);
    }
}
