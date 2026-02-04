#include "metadata.h"

#include <filesystem>
#include <fstream>
#include <vector>

#include "miniaudio.h"

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

static std::string decode_text(const uint8_t* data, size_t size, uint8_t encoding)
{
    if (size == 0)
    {
        return std::string();
    }

    if (encoding == 0 || encoding == 3)
    {
        return std::string(reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data + size));
    }

    std::string result;
    result.reserve(size);
    for (size_t i = 0; i + 1 < size; i += 2)
    {
        if (data[i] == 0 && data[i + 1] == 0)
        {
            break;
        }
        if (data[i + 1] != 0)
        {
            result.push_back(static_cast<char>(data[i + 1]));
        }
        else
        {
            result.push_back(static_cast<char>(data[i]));
        }
    }
    return result;
}

static void parse_id3_text(const std::vector<uint8_t>& tag_data, uint8_t version, track_metadata& metadata)
{
    size_t offset = 0;
    while (offset + 10 <= tag_data.size())
    {
        const uint8_t* frame = tag_data.data() + offset;
        if (frame[0] == 0 && frame[1] == 0 && frame[2] == 0 && frame[3] == 0)
        {
            break;
        }

        uint32_t frame_size = (version == 4)
            ? read_syncsafe32(frame + 4)
            : read_be32(frame + 4);
        if (frame_size == 0)
        {
            break;
        }

        size_t data_offset = offset + 10;
        if (data_offset + frame_size > tag_data.size())
        {
            break;
        }

        const char id0 = static_cast<char>(frame[0]);
        const char id1 = static_cast<char>(frame[1]);
        const char id2 = static_cast<char>(frame[2]);
        const char id3 = static_cast<char>(frame[3]);

        const uint8_t* data = tag_data.data() + data_offset;
        if (frame_size > 1)
        {
            uint8_t encoding = data[0];
            std::string value = decode_text(data + 1, frame_size - 1, encoding);

            if (id0 == 'T' && id1 == 'I' && id2 == 'T' && id3 == '2')
            {
                metadata.title = value;
            }
            else if (id0 == 'T' && id1 == 'P' && id2 == 'E' && id3 == '1')
            {
                metadata.artist = value;
            }
            else if (id0 == 'T' && id1 == 'A' && id2 == 'L' && id3 == 'B')
            {
                metadata.album = value;
            }
            else if ((id0 == 'T' && id1 == 'Y' && id2 == 'E' && id3 == 'R') ||
                     (id0 == 'T' && id1 == 'D' && id2 == 'R' && id3 == 'C'))
            {
                metadata.date = value;
            }
            else if (id0 == 'T' && id1 == 'C' && id2 == 'O' && id3 == 'N')
            {
                metadata.genre = value;
            }
            else if (id0 == 'T' && id1 == 'R' && id2 == 'C' && id3 == 'K')
            {
                metadata.track = value;
            }
        }

        offset += 10 + static_cast<size_t>(frame_size);
    }
}

static void read_id3_tags(const std::string& path, track_metadata& metadata)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return;
    }

    uint8_t header[10] = {};
    file.read(reinterpret_cast<char*>(header), sizeof(header));
    if (file.gcount() != static_cast<std::streamsize>(sizeof(header)))
    {
        return;
    }

    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3')
    {
        return;
    }

    uint8_t version = header[3];
    uint32_t tag_size = read_syncsafe32(&header[6]);
    if (tag_size == 0)
    {
        return;
    }

    std::vector<uint8_t> tag_data(tag_size);
    file.read(reinterpret_cast<char*>(tag_data.data()), static_cast<std::streamsize>(tag_data.size()));
    if (file.gcount() != static_cast<std::streamsize>(tag_data.size()))
    {
        return;
    }

    parse_id3_text(tag_data, version, metadata);
}

bool read_track_metadata(const std::string& path, track_metadata& metadata)
{
    metadata = {};
    metadata.sample_rate = 0;
    metadata.channels = 0;
    metadata.duration_ms = 0;
    metadata.file_size_bytes = 0;
    metadata.bitrate_kbps = 0;

    try
    {
        metadata.file_size_bytes = static_cast<long long>(std::filesystem::file_size(path));
    }
    catch (...)
    {
    }

    ma_decoder decoder;
    ma_result result = ma_decoder_init_file(path.c_str(), NULL, &decoder);
    if (result == MA_SUCCESS)
    {
        metadata.sample_rate = static_cast<int>(decoder.outputSampleRate);
        metadata.channels = static_cast<int>(decoder.outputChannels);

        ma_uint64 frames = 0;
        if (ma_decoder_get_length_in_pcm_frames(&decoder, &frames) == MA_SUCCESS)
        {
            if (decoder.outputSampleRate > 0)
            {
                metadata.duration_ms = static_cast<int>((frames * 1000) / decoder.outputSampleRate);
            }
        }

        ma_decoder_uninit(&decoder);
    }

    if (metadata.duration_ms > 0 && metadata.file_size_bytes > 0)
    {
        double seconds = static_cast<double>(metadata.duration_ms) / 1000.0;
        double kbps = (static_cast<double>(metadata.file_size_bytes) * 8.0 / 1000.0) / seconds;
        metadata.bitrate_kbps = static_cast<int>(kbps + 0.5);
    }

    read_id3_tags(path, metadata);
    return true;
}
