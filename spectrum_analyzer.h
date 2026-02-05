#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <mutex>
#include <vector>

#include "terminal.h"

class Renderer;

class SpectrumAnalyzer
{
public:
    SpectrumAnalyzer();

    void set_location(const glm::ivec2& location);
    void set_size(const glm::ivec2& size);

    void push_samples(const float* interleaved, int frames, int channels);
    void update();
    void draw();

private:
    void ensure_buffer();
    void compute_bands(const std::vector<float>& window);

    glm::ivec2 _location = glm::ivec2(0);
    glm::ivec2 _size = glm::ivec2(0);

    std::mutex _mutex;
    std::vector<float> _ring;
    size_t _ring_head = 0;
    int _fft_size = 256;
    int _band_count = 24;
    std::vector<float> _bands;
};
