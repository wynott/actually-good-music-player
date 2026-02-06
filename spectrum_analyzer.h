#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <mutex>
#include <vector>

#include "actually_good_module.h"
#include "terminal.h"

class Renderer;

class SpectrumAnalyzer : public ActuallyGoodModule
{
public:
    SpectrumAnalyzer();

    void push_samples(const float* interleaved, int frames, int channels);
    void update();
    void draw();
    void set_gain(float gain);

private:
    void ensure_buffer();
    void compute_bands(const std::vector<float>& window);

    std::mutex _mutex;
    std::vector<float> _ring;
    size_t _ring_head = 0;
    int _fft_size = 256;
    int _band_count = 24;
    std::vector<float> _bands;
    float _gain = 1.0f;
};
