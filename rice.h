#pragma once

#include <glm/vec2.hpp>

#include <string>
#include <vector>

#include "terminal.h"
#include "config.h"

#include <glm/vec4.hpp>

class Renderer;

class Rice
{
public:
    Rice();
    void run(const app_config& config);

private:
    void draw_frame(int frame);
    glm::ivec2 get_center() const;
    void ensure_buffer();
    void clear_buffer();
    void load_art(const app_config& config);

    std::vector<Terminal::Character> _buffer;
    glm::ivec2 _size = glm::ivec2(0);
    std::vector<std::string> _lines;
    glm::vec4 _art_colour = glm::vec4(1.0f);
};
