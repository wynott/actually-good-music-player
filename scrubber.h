#pragma once

#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "actually_good_module.h"
#include "config.h"

class Scrubber : public ActuallyGoodModule
{
public:
    Scrubber();
    Scrubber(const glm::ivec2& location, const glm::ivec2& size);
    void set_progress(float progress_01);
    void set_time_ms(int elapsed_ms, int total_ms);
    void set_waveform(const std::vector<float>& amplitudes_01);
    void request_waveform(const std::string& path, int columns);
    float consume_peak_gain();

    void draw(const app_config& config) const;

private:
    float _progress = 0.0f;
    int _elapsed_ms = 0;
    int _total_ms = 0;
    std::vector<float> _waveform;
    mutable std::mutex _waveform_mutex;
    std::atomic<int> _waveform_job_id{0};
    std::atomic<float> _pending_peak_gain{-1.0f};
    int _waveform_samples_per_column = 8;
};
