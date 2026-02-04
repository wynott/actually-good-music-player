#pragma once

#include <string>

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
    long long file_size_bytes;
    int bitrate_kbps;
};

bool read_track_metadata(const std::string& path, track_metadata& metadata);
