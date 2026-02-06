#include "spectrum_analyzer.h"
#include "spdlog/spdlog.h"
#include "spectrum_analyzer.h"

#include <algorithm>
#include <cmath>

#include "app.h"
#include "draw.h"
#include "event.h"

static constexpr float kPi = 3.14159265358979323846f;

SpectrumAnalyzer::SpectrumAnalyzer()
{
    ensure_buffer();
}

void SpectrumAnalyzer::ensure_buffer()
{
    if (_fft_size <= 0)
    {
        _fft_size = 256;
    }
    if (_band_count <= 0)
    {
        _band_count = 24;
    }

    size_t ring_size = static_cast<size_t>(_fft_size * 8);
    if (_ring.size() != ring_size)
    {
        _ring.assign(ring_size, 0.0f);
        _ring_head = 0;
    }

    if (static_cast<int>(_bands.size()) != _band_count)
    {
        _bands.assign(static_cast<size_t>(_band_count), 0.0f);
    }
}

void SpectrumAnalyzer::push_samples(const float* interleaved, int frames, int channels)
{
    if (!interleaved || frames <= 0 || channels <= 0)
    {
        return;
    }

    ensure_buffer();

    std::lock_guard<std::mutex> lock(_mutex);
    size_t ring_size = _ring.size();
    if (ring_size == 0)
    {
        return;
    }

    for (int frame = 0; frame < frames; ++frame)
    {
        float sum = 0.0f;
        const float* base = interleaved + (frame * channels);
        for (int ch = 0; ch < channels; ++ch)
        {
            sum += base[ch];
        }
        float sample = sum / static_cast<float>(channels);

        _ring[_ring_head] = sample;
        _ring_head = (_ring_head + 1) % ring_size;
    }
}

void SpectrumAnalyzer::update()
{
    ensure_buffer();

    std::vector<float> window;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_ring.empty())
        {
            return;
        }

        window.resize(static_cast<size_t>(_fft_size), 0.0f);
        size_t ring_size = _ring.size();
        size_t start = (_ring_head + ring_size - static_cast<size_t>(_fft_size)) % ring_size;
        for (int i = 0; i < _fft_size; ++i)
        {
            size_t index = (start + static_cast<size_t>(i)) % ring_size;
            window[static_cast<size_t>(i)] = _ring[index];
        }
    }

    if (window.empty())
    {
        return;
    }

    if (_fft_size > 1)
    {
        for (int i = 0; i < _fft_size; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(_fft_size - 1);
            float w = 0.5f * (1.0f - std::cos(2.0f * kPi * t));
            window[static_cast<size_t>(i)] *= w;
        }
    }

    compute_bands(window);
}

void SpectrumAnalyzer::compute_bands(const std::vector<float>& window)
{
    if (window.empty() || _fft_size <= 0)
    {
        return;
    }

    int bins = _fft_size / 2;
    if (bins <= 0)
    {
        return;
    }

    std::vector<float> magnitudes(static_cast<size_t>(bins), 0.0f);
    float inv_count = 1.0f / static_cast<float>(_fft_size);

    for (int k = 0; k < bins; ++k)
    {
        float re = 0.0f;
        float im = 0.0f;
        float k_factor = 2.0f * kPi * static_cast<float>(k) / static_cast<float>(_fft_size);
        for (int n = 0; n < _fft_size; ++n)
        {
            float angle = k_factor * static_cast<float>(n);
            float sample = window[static_cast<size_t>(n)];
            re += sample * std::cos(angle);
            im -= sample * std::sin(angle);
        }
        float mag = std::sqrt(re * re + im * im) * inv_count;
        magnitudes[static_cast<size_t>(k)] = mag;
    }

    std::vector<float> bands(static_cast<size_t>(_band_count), 0.0f);
    float min_bin = 1.0f;
    float max_bin = static_cast<float>(bins);
    float band_ratio = max_bin / min_bin;

    for (int i = 0; i < _band_count; ++i)
    {
        float t0 = static_cast<float>(i) / static_cast<float>(_band_count);
        float t1 = static_cast<float>(i + 1) / static_cast<float>(_band_count);
        float start_f = min_bin * std::pow(band_ratio, t0);
        float end_f = min_bin * std::pow(band_ratio, t1);

        int start = std::max(1, static_cast<int>(std::floor(start_f)));
        int end = std::max(start + 1, static_cast<int>(std::ceil(end_f)));
        if (end > bins)
        {
            end = bins;
        }

        float sum = 0.0f;
        int count = 0;
        for (int k = start; k < end; ++k)
        {
            sum += magnitudes[static_cast<size_t>(k)];
            count += 1;
        }
        if (count > 0)
        {
            bands[static_cast<size_t>(i)] = sum / static_cast<float>(count);
        }
    }

    float peak = 0.0f;
    for (float value : bands)
    {
        if (value > peak)
        {
            peak = value;
        }
    }
    if (peak <= 0.0f)
    {
        peak = 1.0f;
    }

    if (static_cast<int>(_bands.size()) != _band_count)
    {
        _bands.assign(static_cast<size_t>(_band_count), 0.0f);
    }

    for (int i = 0; i < _band_count; ++i)
    {
        float normalized = bands[static_cast<size_t>(i)] / peak;
        if (normalized < 0.0f)
        {
            normalized = 0.0f;
        }
        if (normalized > 1.0f)
        {
            normalized = 1.0f;
        }
        float compressed = std::sqrt(normalized);

        float previous = _bands[static_cast<size_t>(i)];
        float next = compressed;
        if (next > previous)
        {
            previous = previous + (next - previous) * 0.6f;
        }
        else
        {
            previous = previous * 0.85f + next * 0.15f;
        }
        _bands[static_cast<size_t>(i)] = previous;
    }
}

void SpectrumAnalyzer::draw()
{
    spdlog::trace("SpectrumAnalyzer::draw()");
    
    if (_size.x <= 0 || _size.y <= 0)
    {
        return;
    }

    Renderer* renderer = Renderer::get();
    if (!renderer)
    {
        return;
    }

    const app_config& config = ActuallyGoodMP::instance().get_config();
    glm::vec4 bar_colour = config.ui_text_fg;
    glm::ivec2 terminal_size = renderer->get_terminal_size();
    if (terminal_size.x <= 0 || terminal_size.y <= 0)
    {
        return;
    }

    int min_x = std::max(0, _location.x);
    int min_y = std::max(0, _location.y);
    int max_x = std::min(terminal_size.x - 1, _location.x + _size.x - 1);
    int max_y = std::min(terminal_size.y - 1, _location.y + _size.y - 1);

    if (min_x > max_x || min_y > max_y)
    {
        return;
    }

    int width = max_x - min_x + 1;
    int height = max_y - min_y + 1;

    for (int y = min_y; y <= max_y; ++y)
    {
        float t = (height > 1) ? static_cast<float>(max_y - y) / static_cast<float>(height - 1) : 0.0f;
        glm::vec4 band_colour = config.spectrum_colour_low + (config.spectrum_colour_high - config.spectrum_colour_low) * t;
        for (int x = min_x; x <= max_x; ++x)
        {
            glm::ivec2 cell_location(x, y);
            renderer->draw_glyph(
                cell_location,
                U' ',
                band_colour,
                glm::vec4(0.0f));
        }
    }

    for (int x = 0; x < width; ++x)
    {
        int band = (x * _band_count) / width;
        if (band < 0)
        {
            band = 0;
        }
        if (band >= static_cast<int>(_bands.size()))
        {
            band = static_cast<int>(_bands.size()) - 1;
        }

        float value = _bands[static_cast<size_t>(band)];
        if (value < 0.0f)
        {
            value = 0.0f;
        }
        float freq_t = (width > 1) ? static_cast<float>(x) / static_cast<float>(width - 1) : 0.0f;
        float weight = 0.25f + 2.25f * freq_t;
        value *= weight;
        if (value > 1.0f)
        {
            value = 1.0f;
        }

        float clamped = std::clamp(value, 0.0f, 1.0f);
        float bar_height_f = clamped * static_cast<float>(height);
        if (bar_height_f <= 0.0f)
        {
            bar_height_f = 0.0f;
        }
        if (bar_height_f > static_cast<float>(height))
        {
            bar_height_f = static_cast<float>(height);
        }
        int full_cells = static_cast<int>(std::floor(bar_height_f));
        float remainder = bar_height_f - static_cast<float>(full_cells);
        if (full_cells >= height)
        {
            full_cells = height;
            remainder = 0.0f;
        }

        float threshold = config.spectrum_particle_threshold;
        if (clamped >= threshold && height > 0)
        {
            int top_y = (remainder > 0.0f) ? full_cells : (full_cells - 1);
            if (top_y < 0)
            {
                top_y = 0;
            }
            int emit_y = max_y - top_y;
            int emit_x = min_x + x;
            EventBus::instance().publish(Event{
                "debug.particle_emit",
                std::to_string(emit_x) + "," + std::to_string(emit_y) + "," + std::to_string(freq_t)});
        }

        glm::vec4 low = config.spectrum_colour_low;
        glm::vec4 high = config.spectrum_colour_high;

        int draw_x = min_x + x;
        auto partial_block = [](float frac)
        {
            if (frac <= 0.0f)
            {
                return U' ';
            }
            if (frac >= 1.0f)
            {
                return U'█';
            }
            static const char32_t levels[] = {U'▁', U'▂', U'▃', U'▅', U'▆', U'▇', U'▉', U'█'};
            int idx = static_cast<int>(std::ceil(frac * 8.0f)) - 1;
            if (idx < 0) idx = 0;
            if (idx > 7) idx = 7;
            return levels[idx];
        };

        for (int y = 0; y < height; ++y)
        {
            int draw_y = max_y - y;
            glm::ivec2 cell_location(draw_x, draw_y);
            if (remainder > 0.0f && y == full_cells)
            {
                float t = (height > 1) ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.0f;
                glm::vec4 band_colour = low + (high - low) * t;
                char32_t glyph = partial_block(remainder);
                renderer->draw_glyph(
                    cell_location,
                    glyph,
                    band_colour,
                    glm::vec4(0.0f));
            }
            else if (remainder <= 0.0f && y == full_cells - 1)
            {
                float t = (height > 1) ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.0f;
                glm::vec4 band_colour = low + (high - low) * t;
                char32_t glyph = partial_block(1.0f);
                renderer->draw_glyph(
                    cell_location,
                    glyph,
                    band_colour,
                    glm::vec4(0.0f));
            }
            else if (y < full_cells)
            {
                float t = (height > 1) ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.0f;
                float step = (height > 1) ? 1.0f / static_cast<float>(height - 1) : 0.0f;
                float t_top = std::min(1.0f, t + step * 0.5f);
                glm::vec4 bottom_colour = low + (high - low) * t;
                glm::vec4 top_colour = low + (high - low) * t_top;
                renderer->draw_glyph(
                    cell_location,
                    U'▄',
                    bottom_colour,
                    top_colour);
            }
            else
            {
                renderer->draw_glyph(
                    cell_location,
                    U' ',
                    glm::vec4(0.0f),
                    glm::vec4(0.0f));
            }
        }
    }
    
    if (width >= 6)
    {
        struct label_info
        {
            float hz;
            const char* text;
        } labels[] = {
            {50.0f, "50"},
            {200.0f, "200"},
            {1000.0f, "1k"},
            {4000.0f, "4k"},
            {16000.0f, "16k"},
        };

        float min_hz = 40.0f;
        float max_hz = 20000.0f;
        float log_min = std::log(min_hz);
        float log_max = std::log(max_hz);
        int label_row = max_y;
        for (const auto& label : labels)
        {
            float log_hz = std::log(label.hz);
            float t = (log_hz - log_min) / (log_max - log_min);
            if (t < 0.0f)
            {
                t = 0.0f;
            }
            if (t > 1.0f)
            {
                t = 1.0f;
            }
            int x = min_x + static_cast<int>(t * static_cast<float>(width - 1) + 0.5f);
            int text_len = static_cast<int>(std::char_traits<char>::length(label.text));
            int max_start = std::max(min_x, max_x - text_len + 1);
            if (x > max_start)
            {
                x = max_start;
            }
            renderer->draw_string(label.text, glm::ivec2(x, label_row));
        }
    }

}
