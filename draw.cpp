
#include <algorithm>
#include <iostream>

#include "draw.h"
#include "spdlog/spdlog.h"
#include "terminal.h"

Renderer* Renderer::_instance = nullptr;

Renderer::Renderer(Terminal& terminal)
    : _terminal(terminal)
{
    if (!_instance)
    {
        _instance = this;
    }
}

Renderer* Renderer::init(Terminal& terminal)
{
    if (!_instance)
    {
        _instance = new Renderer(terminal);
    }
    return _instance;
}

Renderer* Renderer::get()
{
    return _instance;
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

void Renderer::set_canvas(const std::vector<Terminal::Character>& source)
{
    spdlog::trace("Renderer::set_canvas(buffer)");
    _terminal.set_canvas(source);
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

void Renderer::draw_glyph(
    const glm::ivec2& location,
    char32_t glyph,
    const glm::vec4& foreground,
    const glm::vec4& background)
{
    spdlog::trace("Renderer::draw_glyph()");
    _terminal.set_glyph(location, glyph, foreground, background);
}

void Renderer::clear_box(const glm::ivec2& min_corner, const glm::ivec2& size)
{
    spdlog::trace("Renderer::clear_box()");

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

    for (int y = min_y; y <= max_y; ++y)
    {
        for (int x = min_x; x <= max_x; ++x)
        {
            _terminal.clear_cell(glm::ivec2(x, y));
        }
    }
}

glm::ivec2 Renderer::get_terminal_size() const
{
    return _terminal.get_size();
}
