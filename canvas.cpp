#include "canvas.h"

Canvas::Canvas(const glm::ivec2& size)
{
    resize(size);
}

void Canvas::resize(const glm::ivec2& size)
{
    _size = size;
    if (_size.x <= 0 || _size.y <= 0)
    {
        _buffer.clear();
        return;
    }
    _buffer.assign(static_cast<size_t>(_size.x * _size.y), Terminal::Character{});
}

const glm::ivec2& Canvas::get_size() const
{
    return _size;
}

const std::vector<Terminal::Character>& Canvas::get_buffer() const
{
    return _buffer;
}

std::vector<Terminal::Character>* Canvas::mutate_buffer()
{
    return &_buffer;
}

void Canvas::fill(const glm::vec4& colour)
{
    if (_size.x <= 0 || _size.y <= 0)
    {
        return;
    }

    for (Terminal::Character& cell : _buffer)
    {
        cell.set_glyph(U' ');
        cell.set_glyph_colour(colour);
        cell.set_background_colour(colour);
    }
}

void Canvas::fill_gradient(
    const glm::vec4& top_left,
    const glm::vec4& top_right,
    const glm::vec4& bottom_left,
    const glm::vec4& bottom_right)
{
    if (_size.x <= 0 || _size.y <= 0)
    {
        return;
    }

    auto lerp = [](const glm::vec4& a, const glm::vec4& b, float t)
    {
        return a + (b - a) * t;
    };

    int width = _size.x;
    int height = _size.y;
    for (int y = 0; y < height; ++y)
    {
        float v = (height > 1) ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.0f;
        for (int x = 0; x < width; ++x)
        {
            float u = (width > 1) ? static_cast<float>(x) / static_cast<float>(width - 1) : 0.0f;
            glm::vec4 top = lerp(top_left, top_right, u);
            glm::vec4 bottom = lerp(bottom_left, bottom_right, u);
            glm::vec4 colour_vec = lerp(top, bottom, v);

            size_t index = static_cast<size_t>(y * width + x);
            Terminal::Character& cell = _buffer[index];
            cell.set_glyph(U' ');
            glm::vec4 colour(colour_vec.x, colour_vec.y, colour_vec.z, 1.0f);
            cell.set_glyph_colour(colour);
            cell.set_background_colour(colour);
        }
    }
}
