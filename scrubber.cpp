#include "scrubber.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <thread>

#include "draw.h"
#include "miniaudio.h"

Scrubber::Scrubber() = default;

Scrubber::Scrubber(const glm::ivec2& location, const glm::ivec2& size)
{
    set_location(location);
    set_size(size);
}

void Scrubber::set_progress(float progress_01)
{
    _progress = std::clamp(progress_01, 0.0f, 1.0f);
}

void Scrubber::set_time_ms(int elapsed_ms, int total_ms)
{
    _elapsed_ms = std::max(0, elapsed_ms);
    _total_ms = std::max(0, total_ms);
}

void Scrubber::set_waveform(const std::vector<float>& amplitudes_01)
{
    std::vector<float> clamped = amplitudes_01;
    for (float& value : clamped)
    {
        value = std::clamp(value, 0.0f, 1.0f);
    }
    std::lock_guard<std::mutex> lock(_waveform_mutex);
    _waveform = std::move(clamped);
}

static std::vector<float> build_waveform(const std::string& path, int columns)
{
    if (columns <= 0)
    {
        return {};
    }

    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, 48000);
    if (ma_decoder_init_file(path.c_str(), &config, &decoder) != MA_SUCCESS)
    {
        return {};
    }

    ma_uint64 total_frames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &total_frames) != MA_SUCCESS)
    {
        ma_decoder_uninit(&decoder);
        return {};
    }

    if (total_frames == 0)
    {
        ma_decoder_uninit(&decoder);
        return {};
    }

    std::vector<float> peaks(static_cast<size_t>(columns), 0.0f);
    ma_uint64 frames_per_bin = total_frames / static_cast<ma_uint64>(columns);
    if (frames_per_bin == 0)
    {
        frames_per_bin = 1;
    }

    const ma_uint32 channels = decoder.outputChannels;
    const ma_uint64 chunk_frames = 4096;
    std::vector<float> buffer(static_cast<size_t>(chunk_frames * channels));
    ma_uint64 frame_index = 0;
    while (frame_index < total_frames)
    {
        ma_uint64 frames_read = 0;
        if (ma_decoder_read_pcm_frames(&decoder, buffer.data(), chunk_frames, &frames_read) != MA_SUCCESS)
        {
            break;
        }
        if (frames_read == 0)
        {
            break;
        }

        for (ma_uint64 i = 0; i < frames_read; ++i)
        {
            float sample = 0.0f;
            if (channels >= 2)
            {
                float left = buffer[static_cast<size_t>(i * channels)];
                float right = buffer[static_cast<size_t>(i * channels + 1)];
                sample = 0.5f * (std::fabs(left) + std::fabs(right));
            }
            else if (channels == 1)
            {
                sample = std::fabs(buffer[static_cast<size_t>(i)]);
            }

            ma_uint64 absolute_frame = frame_index + i;
            size_t bin = static_cast<size_t>(absolute_frame / frames_per_bin);
            if (bin >= peaks.size())
            {
                bin = peaks.size() - 1;
            }
            if (sample > peaks[bin])
            {
                peaks[bin] = sample;
            }
        }

        frame_index += frames_read;
    }

    ma_decoder_uninit(&decoder);

    float max_peak = 0.0f;
    for (float peak : peaks)
    {
        if (peak > max_peak)
        {
            max_peak = peak;
        }
    }

    if (max_peak <= 1.0e-6f)
    {
        return std::vector<float>(peaks.size(), 0.0f);
    }

    std::vector<float> normalized;
    normalized.reserve(peaks.size());
    for (float peak : peaks)
    {
        float norm = peak / max_peak;
        normalized.push_back(std::clamp(norm, 0.0f, 1.0f));
    }

    return normalized;
}

void Scrubber::request_waveform(const std::string& path, int columns)
{
    if (path.empty())
    {
        set_waveform({});
        return;
    }

    int job_id = _waveform_job_id.fetch_add(1) + 1;
    std::string path_copy = path;
    std::thread([this, path_copy, columns, job_id]()
    {
        std::vector<float> waveform = build_waveform(path_copy, columns);
        if (_waveform_job_id.load() != job_id)
        {
            return;
        }
        set_waveform(waveform);
    }).detach();
}

void Scrubber::draw(const app_config& config) const
{
    auto renderer = Renderer::get();
    if (!renderer)
    {
        return;
    }

    if (_size.x <= 0 || _size.y <= 0)
    {
        return;
    }

    glm::ivec2 terminal_size = renderer->get_terminal_size();
    int max_width = std::max(0, terminal_size.x - _location.x);
    int max_height = std::max(0, terminal_size.y - _location.y);
    glm::ivec2 draw_size(
        std::min(_size.x, max_width),
        std::min(_size.y, max_height));
    if (draw_size.x <= 0 || draw_size.y <= 0)
    {
        return;
    }

    glm::ivec2 actual_size = renderer->draw_box(_location, draw_size, config.ui_box_fg, glm::vec4(0.0f));

    int inner_width = actual_size.x - 2;
    int inner_height = actual_size.y - 3;
    if (inner_width <= 0 || inner_height <= 0)
    {
        return;
    }

    auto format_time = [](int ms)
    {
        int total_seconds = std::max(0, ms / 1000);
        int minutes = total_seconds / 60;
        int seconds = total_seconds % 60;
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "%d:%02d", minutes, seconds);
        return std::string(buffer);
    };

    std::string elapsed_text = format_time(_elapsed_ms);
    std::string remaining_text = format_time(std::max(0, _total_ms - _elapsed_ms));
    if (remaining_text != "0:00")
    {
        remaining_text.insert(remaining_text.begin(), '-');
    }

    renderer->draw_string(
        elapsed_text,
        glm::ivec2(_location.x + 1, _location.y + 1));
    int right_x = _location.x + actual_size.x - 1 - static_cast<int>(remaining_text.size());
    if (right_x >= _location.x + 1)
    {
        renderer->draw_string(
            remaining_text,
            glm::ivec2(right_x, _location.y + 1));
    }

    int origin_x = _location.x + 1;
    int origin_y = _location.y + 2;

    int progress_x = origin_x + static_cast<int>(std::round(_progress * (inner_width - 1)));
    progress_x = std::clamp(progress_x, origin_x, origin_x + inner_width - 1);

    std::vector<float> waveform;
    {
        std::lock_guard<std::mutex> lock(_waveform_mutex);
        waveform = _waveform;
    }

    auto compute_fill_ratio = [&](float exponent)
    {
        if (waveform.empty() || inner_width <= 0 || inner_height <= 0)
        {
            return 0.0f;
        }

        double filled = 0.0;
        double total = static_cast<double>(inner_width) * static_cast<double>(inner_height);
        for (int x = 0; x < inner_width; ++x)
        {
            float amplitude = 0.0f;
            size_t index = static_cast<size_t>(
                std::round(static_cast<float>(x) / static_cast<float>(inner_width - 1) * static_cast<float>(waveform.size() - 1)));
            if (index < waveform.size())
            {
                amplitude = waveform[index];
            }

            float shaped = std::pow(std::clamp(amplitude, 0.0f, 1.0f), exponent);
            int bar_height = std::max(1, static_cast<int>(std::round(shaped * static_cast<float>(inner_height))));
            filled += static_cast<double>(bar_height);
        }

        return static_cast<float>(filled / total);
    };

    float low_exp = 0.2f;
    float high_exp = 50.0f;
    float target_ratio = 0.5f;
    float exponent = 22.0f;
    for (int i = 0; i < 10; ++i)
    {
        float ratio = compute_fill_ratio(exponent);
        if (ratio > target_ratio)
        {
            low_exp = exponent;
        }
        else
        {
            high_exp = exponent;
        }
        exponent = 0.5f * (low_exp + high_exp);
    }

    for (int x = 0; x < inner_width; ++x)
    {
        float amplitude = 0.0f;
        if (!waveform.empty())
        {
            size_t index = static_cast<size_t>(
                std::round(static_cast<float>(x) / static_cast<float>(inner_width - 1) * static_cast<float>(waveform.size() - 1)));
            if (index < waveform.size())
            {
                amplitude = waveform[index];
            }
        }

        float shaped = std::pow(std::clamp(amplitude, 0.0f, 1.0f), exponent);
        int bar_height = std::max(1, static_cast<int>(std::round(shaped * static_cast<float>(inner_height))));
        int column_x = origin_x + x;
        for (int y = 0; y < inner_height; ++y)
        {
            int row_y = origin_y + (inner_height - 1 - y);
            float t = (inner_height > 1) ? static_cast<float>(y) / static_cast<float>(inner_height - 1) : 0.0f;
            glm::vec4 low = config.scrubber_colour_low;
            glm::vec4 high = config.scrubber_colour_high;
            glm::vec4 fg = low + (high - low) * t;
            glm::vec4 bg(0.0f);
            if (y < bar_height)
            {
                renderer->draw_glyph(glm::ivec2(column_x, row_y), U'█', fg, bg);
            }
            else
            {
                renderer->draw_glyph(
                    glm::ivec2(column_x, row_y),
                    U' ',
                    glm::vec4(0.0f),
                    glm::vec4(0.0f));
            }
        }
    }

    for (int y = 0; y < inner_height; ++y)
    {
        int row_y = origin_y + y;
        renderer->draw_glyph(
            glm::ivec2(progress_x, row_y),
            U'│',
            glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            glm::vec4(0.0f));
    }
}
