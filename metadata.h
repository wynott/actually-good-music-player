#pragma once

#include <string>

#include <glm/vec2.hpp>

#include <glm/vec4.hpp>

#include "actually_good_module.h"
#include "config.h"

class Renderer;

struct track_metadata
{
    std::string title;
    std::string artist;
    std::string album;
    std::string date;
    std::string genre;
    std::string track;
    int sample_rate;
    int channels;
    int duration_ms;
    int64_t file_size_bytes;
    int bitrate_kbps;
};

bool read_track_metadata(const std::string& path, track_metadata& metadata);

class MetadataPanel : public ActuallyGoodModule
{
public:
    MetadataPanel();
    MetadataPanel(const glm::ivec2& location, const glm::ivec2& size);

    void draw(const app_config& config, const track_metadata& meta);

};
