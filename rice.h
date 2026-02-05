#pragma once

#include <glm/vec2.hpp>

#include <string>
#include <vector>

#include "terminal.h"
#include "config.h"

class Renderer;

class Rice
{
public:
    Rice();
    void run(const app_config& config);
    void set_terminal(Terminal* terminal);
    void set_renderer(Renderer* renderer);

private:
    void draw_frame(int frame);
    glm::ivec2 get_center() const;
    void ensure_buffer();
    void clear_buffer();
    void load_art();

    std::vector<Terminal::Character> _buffer;
    glm::ivec2 _size = glm::ivec2(0);
    std::vector<std::string> _lines;
    glm::vec3 _art_colour = glm::vec3(1.0f);
    Terminal* _terminal = nullptr;
    Renderer* _renderer = nullptr;
};

inline const std::vector<std::string> kRiceArt = {
    "                                                                          ",
    "     ▄▄                         ▄▄ ▄▄          ▄   ▄▄▄▄                   ",
    "   ▄█▀▀█▄         █▄             ██ ██         ▀██████▀                █▄ ",
    "   ██  ██        ▄██▄            ██ ██           ██   ▄                ██ ",
    "   ██▀▀██   ▄███▀ ██ ██ ██ ▄▀▀█▄ ██ ██ ██ ██     ██  ██ ▄███▄ ▄███▄ ▄████ ",
    " ▄ ██  ██   ██    ██ ██ ██ ▄█▀██ ██ ██ ██▄██     ██  ██ ██ ██ ██ ██ ██ ██ ",
    " ▀██▀  ▀█▄█▄▀███▄▄██▄▀██▀█▄▀█▄██▄██▄██▄▄▀██▀     ▀█████▄▀███▀▄▀███▀▄█▀███ ",
    "                                         ██      ▄   ██                   ",
    "                                       ▀▀▀       ▀████▀                   ",
    "                                                                          ",
    "  ▄▄▄     ▄▄▄                          ▄▄▄▄▄▄  ▄▄                         ",
    "   ███▄ ▄███                          █▀██▀▀▀█▄ ██                        ",
    "   ██ ▀█▀ ██               ▀▀           ██▄▄▄█▀ ██                   ▄    ",
    "   ██     ██   ██ ██ ▄██▀█ ██ ▄███▀     ██▀▀▀   ██ ▄▀▀█▄ ██ ██ ▄█▀█▄ ████▄",
    "   ██     ██   ██ ██ ▀███▄ ██ ██      ▄ ██      ██ ▄█▀██ ██▄██ ██▄█▀ ██   ",
    " ▀██▀     ▀██▄▄▀██▀██▄▄██▀▄██▄▀███▄   ▀██▀     ▄██▄▀█▄██▄▄▀██▀▄▀█▄▄▄▄█▀   ",
    "                                                           ██             ",
    "                                                         ▀▀▀              "
};
