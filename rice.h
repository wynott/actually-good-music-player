#pragma once

#include <glm/vec2.hpp>

#include <string>
#include <vector>

#include "terminal.h"
#include "config.h"

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
    void blit();
    void load_art();

    std::vector<Terminal::Character> _buffer;
    glm::ivec2 _size = glm::ivec2(0);
    std::vector<std::string> _lines;
    glm::vec3 _art_colour = glm::vec3(255.0f);
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
