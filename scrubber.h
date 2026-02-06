#pragma once

#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "config.h"

class Scrubber
{
public:
    Scrubber();
    Scrubber(const glm::ivec2& location, const glm::ivec2& size);

    void set_location(const glm::ivec2& location);
    void set_size(const glm::ivec2& size);
    void set_progress(float progress_01);
    void set_waveform(const std::vector<float>& amplitudes_01);
    void request_waveform(const std::string& path, int columns);

    void draw(const app_config& config) const;

private:
    glm::ivec2 _location = glm::ivec2(0);
    glm::ivec2 _size = glm::ivec2(0);
    float _progress = 0.0f;
    std::vector<float> _waveform;
    mutable std::mutex _waveform_mutex;
    std::atomic<int> _waveform_job_id{0};
};
