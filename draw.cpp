
#include <algorithm>
#include <iostream>

#include "draw.h"
#include "spdlog/spdlog.h"
#include "terminal.h"

Renderer::Renderer(Terminal& terminal)
    : _terminal(terminal)
{
}

const Terminal& Renderer::get_terminal() const
{
    return _terminal;
}

void Renderer::set_config(const app_config* config)
{
    _config = config;
}

static glm::vec4 to_vec4(const app_config::rgb_color& color)
{
    return glm::vec4(color.r, color.g, color.b, 1.0f);
}

static glm::vec4 default_box_colour(const app_config* config)
{
    if (config)
    {
        return to_vec4(config->ui_box_fg);
    }
    return glm::vec4(1.0f);
}

static glm::vec4 default_text_colour(const app_config* config)
{
    if (config)
    {
        return to_vec4(config->ui_text_fg);
    }
    return glm::vec4(1.0f);
}

void Renderer::set_canvas(const glm::vec4& colour)
{
    set_canvas(colour, colour, colour, colour);
}

void Renderer::set_canvas(const glm::vec4& top_left, const glm::vec4& top_right, const glm::vec4& bottom_left, const glm::vec4& bottom_right)
{
    spdlog::trace("Renderer::set_canvas()");

    glm::ivec2 size = _terminal.get_size();
    if (size.x <= 0 || size.y <= 0)
    {
        return;
    }

    auto lerp = [](const glm::vec4& a, const glm::vec4& b, float t)
    {
        return a + (b - a) * t;
    };

    int width = size.x;
    int height = size.y;
    std::vector<Terminal::Character> canvas;
    canvas.assign(static_cast<size_t>(width * height), Terminal::Character{});
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
            Terminal::Character& cell = canvas[index];
            cell.set_glyph(U' ');
            glm::vec4 colour(colour_vec.x, colour_vec.y, colour_vec.z, 1.0f);
            cell.set_glyph_colour(colour);
            cell.set_background_colour(colour);
        }
    }

    _terminal.set_canvas(canvas);
}

void Renderer::draw_box(
    const glm::ivec2& min_corner,
    const glm::ivec2& size,
    const glm::vec4& foreground,
    const glm::vec4& background)
{
    spdlog::trace("Renderer::draw_box()");

    if (size.x <= 0 || size.y <= 0)
    {
        return;
    }

    glm::ivec2 terminal_size = _terminal.get_size();
    if (terminal_size.x <= 0 || terminal_size.y <= 0)
    {
        return;
    }

    int min_x = std::max(0, min_corner.x);
    int min_y = std::max(0, min_corner.y);
    int max_x = std::min(terminal_size.x - 1, min_corner.x + size.x - 1);
    int max_y = std::min(terminal_size.y - 1, min_corner.y + size.y - 1);

    if (min_x > max_x || min_y > max_y)
    {
        return;
    }

    auto set_cell = [&](int x, int y, char32_t glyph)
    {
        if (x < min_x || x > max_x || y < min_y || y > max_y)
        {
            return;
        }

        _terminal.set_glyph(
            glm::ivec2(x, y),
            glyph,
            foreground,
            background);
    };

    if (min_x == max_x && min_y == max_y)
    {
        set_cell(min_x, min_y, U'╭');
        return;
    }

    if (min_y == max_y)
    {
        for (int x = min_x; x <= max_x; ++x)
        {
            char32_t glyph = U'─';
            if (x == min_x)
            {
                glyph = U'╭';
            }
            else if (x == max_x)
            {
                glyph = U'╮';
            }
            set_cell(x, min_y, glyph);
        }
        return;
    }

    if (min_x == max_x)
    {
        for (int y = min_y; y <= max_y; ++y)
        {
            char32_t glyph = U'│';
            if (y == min_y)
            {
                glyph = U'╭';
            }
            else if (y == max_y)
            {
                glyph = U'╰';
            }
            set_cell(min_x, y, glyph);
        }
        return;
    }

    set_cell(min_x, min_y, U'╭');
    set_cell(max_x, min_y, U'╮');
    set_cell(min_x, max_y, U'╰');
    set_cell(max_x, max_y, U'╯');

    for (int x = min_x + 1; x < max_x; ++x)
    {
        set_cell(x, min_y, U'─');
        set_cell(x, max_y, U'─');
    }

    for (int y = min_y + 1; y < max_y; ++y)
    {
        set_cell(min_x, y, U'│');
        set_cell(max_x, y, U'│');
    }
}

void Renderer::draw_string(const std::string& text, const glm::ivec2& location)
{
    spdlog::trace("Renderer::draw_string()");
    if (text.empty())
    {
        return;
    }

    glm::ivec2 terminal_size = _terminal.get_size();
    if (terminal_size.x <= 0 || terminal_size.y <= 0)
    {
        return;
    }

    int y = location.y;
    if (y < 0 || y >= terminal_size.y)
    {
        return;
    }

    int x = location.x;
    if (x >= terminal_size.x)
    {
        return;
    }

    int start_x = std::max(0, x);
    size_t skip = (x < 0) ? static_cast<size_t>(-x) : 0u;

    for (size_t i = skip; i < text.size() && start_x < terminal_size.x; ++i, ++start_x)
    {
        glm::ivec2 cell_location(start_x, y);
        _terminal.set_glyph(
            cell_location,
            static_cast<char32_t>(static_cast<unsigned char>(text[i])),
            default_text_colour(_config),
            glm::vec4(0.0f));
    }
}

void Renderer::draw_string_selected(const std::string& text, const glm::ivec2& location)
{
    spdlog::trace("Renderer::draw_string_selected()");

    if (text.empty())
    {
        return;
    }

    glm::ivec2 terminal_size = _terminal.get_size();
    if (terminal_size.x <= 0 || terminal_size.y <= 0)
    {
        return;
    }

    int y = location.y;
    if (y < 0 || y >= terminal_size.y)
    {
        return;
    }

    int x = location.x;
    if (x >= terminal_size.x)
    {
        return;
    }

    int start_x = std::max(0, x);
    size_t skip = (x < 0) ? static_cast<size_t>(-x) : 0u;

    for (size_t i = skip; i < text.size() && start_x < terminal_size.x; ++i, ++start_x)
    {
        glm::ivec2 cell_location(start_x, y);
        _terminal.set_glyph(
            cell_location,
            static_cast<char32_t>(static_cast<unsigned char>(text[i])),
            default_text_colour(_config),
            glm::vec4(0.0f));
    }
}

void Renderer::draw_string_coloured(
    const std::string& text,
    const glm::ivec2& location,
    const glm::vec4& foreground,
    const glm::vec4& background)
{
    spdlog::trace("Renderer::draw_string_coloured()");

    if (text.empty())
    {
        return;
    }

    glm::ivec2 terminal_size = _terminal.get_size();
    if (terminal_size.x <= 0 || terminal_size.y <= 0)
    {
        return;
    }

    int y = location.y;
    if (y < 0 || y >= terminal_size.y)
    {
        return;
    }

    int x = location.x;
    if (x >= terminal_size.x)
    {
        return;
    }

    int start_x = std::max(0, x);
    size_t skip = (x < 0) ? static_cast<size_t>(-x) : 0u;

    for (size_t i = skip; i < text.size() && start_x < terminal_size.x; ++i, ++start_x)
    {
        _terminal.set_glyph(
            glm::ivec2(start_x, y),
            static_cast<char32_t>(static_cast<unsigned char>(text[i])),
            foreground,
            background);
    }
}

void Renderer::draw_string_canvas_bg(
    const std::string& text,
    const glm::ivec2& location,
    const glm::vec4& foreground)
{
    spdlog::trace("Renderer::draw_string_canvas_bg()");

    if (text.empty())
    {
        return;
    }

    glm::ivec2 terminal_size = _terminal.get_size();
    if (terminal_size.x <= 0 || terminal_size.y <= 0)
    {
        return;
    }

    int y = location.y;
    if (y < 0 || y >= terminal_size.y)
    {
        return;
    }

    int x = location.x;
    if (x >= terminal_size.x)
    {
        return;
    }

    int start_x = std::max(0, x);
    size_t skip = (x < 0) ? static_cast<size_t>(-x) : 0u;

    for (size_t i = skip; i < text.size() && start_x < terminal_size.x; ++i, ++start_x)
    {
        glm::ivec2 cell_location(start_x, y);
        _terminal.set_glyph(
            cell_location,
            static_cast<char32_t>(static_cast<unsigned char>(text[i])),
            foreground,
            glm::vec4(0.0f));
    }
}
