#pragma once

#include <vector>

#include "config.h"
#include "terminal.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

class AlbumArt;

class Canvas
{
public:
    Canvas() = default;
    explicit Canvas(const glm::ivec2& size);

    void resize(const glm::ivec2& size);
    const glm::ivec2& get_size() const;
    const std::vector<Terminal::Character>& get_buffer() const;
    std::vector<Terminal::Character>* mutate_buffer();

    void fill(const glm::vec4& colour);
    void fill_gradient(
        const glm::vec4& top_left,
        const glm::vec4& top_right,
        const glm::vec4& bottom_left,
        const glm::vec4& bottom_right);
    void build_default(const app_config& config);
    void build_from_album(const app_config& config, const AlbumArt& album_art);
    void build_grid(const app_config& config);
    void build_logo(const app_config& config);
    void set_origin(const glm::ivec2& origin);

private:
    void draw_art(const std::vector<std::string>& lines, const glm::vec4& colour);
    glm::ivec2 _size = glm::ivec2(0);
    glm::ivec2 _origin = glm::ivec2(0);
    std::vector<Terminal::Character> _buffer;
};
