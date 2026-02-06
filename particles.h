#pragma once

#include <vector>

struct app_config;

class ParticleSystem
{
public:
    ParticleSystem();
    ~ParticleSystem();

    void update(float dt_seconds);
    void draw(const app_config& config) const;
    void clear();
    void emit_debug(int x, int y, float norm_x);
    void set_angle_bias(float bias);

private:
    struct Particle
    {
        float x = 0.0f;
        float y = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        float life = 0.0f;
    };

    std::vector<Particle> _particles;
    int _debug_subscription = 0;
    float _angle_bias = 12.0f;
}; 
