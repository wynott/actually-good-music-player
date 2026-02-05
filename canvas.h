#pragma once

#include <vector>

#include "terminal.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

class Canvas
{
public:
    Canvas() = default;
    explicit Canvas(const glm::ivec2& size);

    void resize(const glm::ivec2& size);
    const glm::ivec2& get_size() const;
    const std::vector<Terminal::Character>& get_buffer() const;

    void fill(const glm::vec4& colour);
    void fill_gradient(
        const glm::vec4& top_left,
        const glm::vec4& top_right,
        const glm::vec4& bottom_left,
        const glm::vec4& bottom_right);

private:
    glm::ivec2 _size = glm::ivec2(0);
    std::vector<Terminal::Character> _buffer;
};
