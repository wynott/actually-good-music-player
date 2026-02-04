#include "album_art.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "album_art.h"
#include "terminal.h"
#include "vendor/stb_image.h"

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

            size_t dst_index = static_cast<size_t>(y * out_w + x);
            Terminal::Character& cell = _pixels[dst_index];
            cell.set_glyph(U'█');
            cell.set_foreground_colour(glm::vec3(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b)));
            cell.set_background_colour(glm::vec3(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b)));
        }
    }

    stbi_image_free(pixels);
    return true;
}

void AlbumArt::draw() const
{
    if (_pixels.empty() || _size.x <= 0 || _size.y <= 0)
    {
        return;
    }

    Terminal& terminal = Terminal::instance();
    glm::ivec2 terminal_size = terminal.get_size();
    if (terminal_size.x <= 0 || terminal_size.y <= 0)
    {
        return;
    }

    std::vector<Terminal::Character>& buffer = terminal.mutate_buffer();
    int width = terminal_size.x;

    for (int y = 0; y < _size.y; ++y)
    {
        int target_y = _location.y + y;
        if (target_y < 0 || target_y >= terminal_size.y)
        {
            continue;
        }

        for (int x = 0; x < _size.x; ++x)
        {
            int target_x = _location.x + x;
            if (target_x < 0 || target_x >= terminal_size.x)
            {
                continue;
            }

            size_t src_index = static_cast<size_t>(y * _size.x + x);
            if (src_index >= _pixels.size())
            {
                continue;
            }

            size_t dst_index = static_cast<size_t>(target_y * width + target_x);
            if (dst_index >= buffer.size())
            {
                continue;
            }

            const Terminal::Character& src = _pixels[src_index];
            Terminal::Character& dst = buffer[dst_index];
            dst.set_glyph(src.get_glyph());
            dst.set_foreground_colour(src.get_foreground_colour());
            dst.set_background_colour(src.get_background_colour());
        }
    }
}
