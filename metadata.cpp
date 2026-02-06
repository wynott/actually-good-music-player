#include "metadata.h"

#include <filesystem>
#include <fstream>
#include <vector>

#include "draw.h"
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

    bool little_endian = true;
    size_t offset = 0;
    if (encoding == 1 && size >= 2)
    {
        uint16_t bom = static_cast<uint16_t>(data[0] << 8) | static_cast<uint16_t>(data[1]);
        if (bom == 0xFEFF)
        {
            little_endian = false;
            offset = 2;
        }
        else if (bom == 0xFFFE)
        {
            little_endian = true;
            offset = 2;
        }
    }
    else if (encoding == 2)
    {
        little_endian = false;
    }

    std::string result;
    result.reserve(size / 2);
    for (size_t i = offset; i + 1 < size; i += 2)
    {
        uint16_t code = little_endian
            ? static_cast<uint16_t>(data[i] | (data[i + 1] << 8))
            : static_cast<uint16_t>((data[i] << 8) | data[i + 1]);
        if (code == 0)
        {
            break;
        }
        if (code <= 0xFF)
        {
            result.push_back(static_cast<char>(code));
        }
        else
        {
            result.push_back('?');
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

    file.seekg(0, std::ios::end);
    std::streamoff file_size = file.tellg();
    file.seekg(0, std::ios::beg);

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
    const uint32_t max_tag_size = 16 * 1024 * 1024;
    if (tag_size == 0 || tag_size > max_tag_size)
    {
        return;
    }
    if (file_size > 0 && static_cast<std::streamoff>(tag_size) > file_size - 10)
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

MetadataPanel::MetadataPanel() = default;

MetadataPanel::MetadataPanel(const glm::ivec2& location, const glm::ivec2& size)
{
    set_location(location);
    set_size(size);
}

void MetadataPanel::draw(const app_config& config, const track_metadata& meta)
{
    if (_size.x <= 0 || _size.y <= 0)
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

    int inner_width = std::max(0, _size.x - 2);
    int inner_height = std::max(0, _size.y - 2);

    glm::ivec2 actual_size(0);
    if (auto renderer = Renderer::get())
    {
        actual_size = renderer->draw_box(
            _location,
            _size,
            glm::vec4(1.0f),
            glm::vec4(0.0f));
    }

    inner_width = std::max(0, actual_size.x - 2);
    inner_height = std::max(0, actual_size.y - 2);

    int max_lines = std::min(inner_height, static_cast<int>(lines.size()));
    for (int i = 0; i < max_lines; ++i)
    {
        std::string line = lines[static_cast<size_t>(i)];
        int max_width = inner_width;
        if (config.metadata_max_width > 0)
        {
            max_width = std::min(max_width, config.metadata_max_width);
        }
        if (max_width > 0 && static_cast<int>(line.size()) > max_width)
        {
            line.resize(static_cast<size_t>(max_width));
        }
        
        if (auto renderer = Renderer::get())
        {
            renderer->draw_string(line, glm::ivec2(_location.x + 1, _location.y + 1 + i));
        }
    }
}
