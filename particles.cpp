#include "particles.h"

#include <algorithm>
#include <random>

#include "config.h"
#include "draw.h"
#include "event.h"
#include "spdlog/spdlog.h"

ParticleSystem::ParticleSystem()
{
    _debug_subscription = EventBus::instance().subscribe(
        "debug.particle_emit",
        [this](const Event& event)
        {
            if (event.payload.empty())
            {
                return;
            }

            size_t comma = event.payload.find(',');
            if (comma == std::string::npos)
            {
                return;
            }
            size_t comma2 = event.payload.find(',', comma + 1);

            int x = 0;
            int y = 0;
            float norm_x = 0.5f;
            try
            {
                x = std::stoi(event.payload.substr(0, comma));
                if (comma2 == std::string::npos)
                {
                    y = std::stoi(event.payload.substr(comma + 1));
                }
                else
                {
                    y = std::stoi(event.payload.substr(comma + 1, comma2 - comma - 1));
                    norm_x = std::stof(event.payload.substr(comma2 + 1));
                }
            }
            catch (...)
            {
                return;
            }

            spdlog::info("particle event");
            emit_debug(x, y, norm_x);
        });
}

ParticleSystem::~ParticleSystem()
{
    if (_debug_subscription != 0)
    {
        EventBus::instance().unsubscribe(_debug_subscription);
        _debug_subscription = 0;
    }
}

void ParticleSystem::update(float dt_seconds)
{
    if (dt_seconds <= 0.0f)
    {
        return;
    }

    for (auto& particle : _particles)
    {
        particle.vy += 40.0f * dt_seconds;
        particle.x += particle.vx * dt_seconds;
        particle.y += particle.vy * dt_seconds;
    }

    auto renderer = Renderer::get();
    if (!renderer)
    {
        return;
    }

    renderer->clear_juice();

    glm::ivec2 size = renderer->get_terminal_size();
    if (size.y <= 0)
    {
        return;
    }

    float max_y = static_cast<float>(size.y);
    _particles.erase(
        std::remove_if(
            _particles.begin(),
            _particles.end(),
            [max_y](const Particle& particle)
            {
                return particle.y >= max_y;
            }),
        _particles.end());
}

void ParticleSystem::draw(const app_config& config) const
{
    auto renderer = Renderer::get();
    if (!renderer)
    {
        return;
    }

    glm::ivec2 size = renderer->get_terminal_size();
    if (size.x <= 0 || size.y <= 0)
    {
        return;
    }

    glm::vec4 top_color = config.spectrum_colour_high;
    glm::vec4 bottom_color = config.spectrum_colour_low;
    for (size_t i = 0; i < _particles.size(); ++i)
    {
        const auto& particle = _particles[i];
        int x = static_cast<int>(particle.x);
        int y = static_cast<int>(particle.y);
        if (x < 0 || y < 0 || x >= size.x || y >= size.y)
        {
            continue;
        }
        float vx = particle.vx;
        float vy = particle.vy;
        char32_t glyph = U'•';
        float mag = std::sqrt(vx * vx + vy * vy);
        if (mag > 0.001f)
        {
            float angle = std::atan2(-vy, vx);
            if (angle < 0.0f)
            {
                angle += 6.2831853f;
            }

            static const char32_t arrows[8] = {U'→', U'↗', U'↑', U'↖', U'←', U'↙', U'↓', U'↘'};
            int index = static_cast<int>(std::floor((angle + 0.3926991f) / 0.7853982f)) % 8;
            glyph = arrows[index];
        }
        float t = (size.y > 1) ? static_cast<float>(y) / static_cast<float>(size.y - 1) : 0.0f;
        glm::vec4 color = top_color + (bottom_color - top_color) * t;
        uint32_t particle_id = static_cast<uint32_t>(i + 1);
        renderer->draw_particle_glyph(glm::ivec2(x, y), glyph, color, glm::vec4(0.0f), particle_id);
    }
}

void ParticleSystem::clear()
{
    _particles.clear();
}

void ParticleSystem::emit_debug(int x, int y, float norm_x)
{
    Particle particle;
    particle.x = static_cast<float>(x);
    particle.y = static_cast<float>(y);
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> drift(-8.0f, 8.0f);
    float bias = (std::clamp(norm_x, 0.0f, 1.0f) * 2.0f - 1.0f) * _angle_bias;
    particle.vx = bias + drift(rng);
    particle.vy = -30.0f;
    particle.life = 0.6f;
    _particles.push_back(particle);
}

void ParticleSystem::set_angle_bias(float bias)
{
    _angle_bias = std::max(0.0f, bias);
}
