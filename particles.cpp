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

            int x = 0;
            int y = 0;
            try
            {
                x = std::stoi(event.payload.substr(0, comma));
                y = std::stoi(event.payload.substr(comma + 1));
            }
            catch (...)
            {
                return;
            }

            spdlog::info("particle event");
            emit_debug(x, y);
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

    glm::vec4 color = config.spectrum_colour_high;
    for (const auto& particle : _particles)
    {
        int x = static_cast<int>(particle.x);
        int y = static_cast<int>(particle.y);
        if (x < 0 || y < 0 || x >= size.x || y >= size.y)
        {
            continue;
        }
        renderer->draw_glyph(glm::ivec2(x, y), U'*', color, glm::vec4(0.0f));
    }
}

void ParticleSystem::clear()
{
    _particles.clear();
}

void ParticleSystem::emit_debug(int x, int y)
{
    Particle particle;
    particle.x = static_cast<float>(x);
    particle.y = static_cast<float>(y);
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> drift(-8.0f, 8.0f);
    particle.vx = drift(rng);
    particle.vy = -30.0f;
    particle.life = 0.6f;
    _particles.push_back(particle);
}
