#include "album_art.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

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
    if (tag_size == 0)
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
