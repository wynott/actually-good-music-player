
#include <algorithm>

#include "draw.h"
#include "app.h"
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

static glm::vec4 default_text_colour()
{
    const app_config& config = ActuallyGoodMP::instance().get_config();
    return config.ui_text_fg;
}

void Renderer::set_canvas(const std::vector<Terminal::Character>& source)
{
    spdlog::trace("Renderer::set_canvas() begin");

    _terminal.set_canvas(source);

    spdlog::trace("Renderer::set_canvas() end");
}

glm::ivec2 Renderer::draw_box(
    const glm::ivec2& min_corner,
    const glm::ivec2& size,
    const glm::vec4& foreground,
    const glm::vec4& background)
{
    spdlog::trace("Renderer::draw_box()");

    if (size.x <= 0 || size.y <= 0)
    {
        return glm::ivec2(0);
    }

    glm::ivec2 terminal_size = _terminal.get_size();
    if (terminal_size.x <= 0 || terminal_size.y <= 0)
    {
        return glm::ivec2(0);
    }

    int min_x = std::max(0, min_corner.x);
    int min_y = std::max(0, min_corner.y);
    int max_x = std::min(terminal_size.x - 1, min_corner.x + size.x - 1);
    int max_y = std::min(terminal_size.y - 1, min_corner.y + size.y - 1);

    if (min_x > max_x || min_y > max_y)
    {
        return glm::ivec2(0);
    }

    glm::ivec2 actual_size(max_x - min_x + 1, max_y - min_y + 1);

    glm::vec4 transparent_bg(0.0f);

    if (min_x == max_x && min_y == max_y)
    {
        _terminal.set_glyph(glm::ivec2(min_x, min_y), U'╭', foreground, transparent_bg);
        return actual_size;
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
            _terminal.set_glyph(glm::ivec2(x, min_y), glyph, foreground, transparent_bg);
        }
        return actual_size;
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
            _terminal.set_glyph(glm::ivec2(min_x, y), glyph, foreground, transparent_bg);
        }
        return actual_size;
    }

    _terminal.set_glyph(glm::ivec2(min_x, min_y), U'╭', foreground, transparent_bg);
    _terminal.set_glyph(glm::ivec2(max_x, min_y), U'╮', foreground, transparent_bg);
    _terminal.set_glyph(glm::ivec2(min_x, max_y), U'╰', foreground, transparent_bg);
    _terminal.set_glyph(glm::ivec2(max_x, max_y), U'╯', foreground, transparent_bg);

    for (int x = min_x + 1; x < max_x; ++x)
    {
        _terminal.set_glyph(glm::ivec2(x, min_y), U'─', foreground, transparent_bg);
        _terminal.set_glyph(glm::ivec2(x, max_y), U'─', foreground, transparent_bg);
    }

    for (int y = min_y + 1; y < max_y; ++y)
    {
        _terminal.set_glyph(glm::ivec2(min_x, y), U'│', foreground, transparent_bg);
        _terminal.set_glyph(glm::ivec2(max_x, y), U'│', foreground, transparent_bg);
    }

    return actual_size;
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
            default_text_colour(),
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
            default_text_colour(),
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

void Renderer::draw_particle_glyph(
    const glm::ivec2& location,
    char32_t glyph,
    const glm::vec4& foreground,
    const glm::vec4& background,
    uint32_t particle_id)
{
    spdlog::trace("Renderer::draw_particle_glyph()");
    _terminal.set_particle_glyph(location, glyph, foreground, background, particle_id);
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

void Renderer::select_region(const glm::ivec2& min_corner, const glm::ivec2& size)
{
    _terminal.select_region(min_corner, size);
}

void Renderer::deselect_region(const glm::ivec2& min_corner, const glm::ivec2& size)
{
    _terminal.deselect_region(min_corner, size);
}

glm::ivec2 Renderer::get_terminal_size() const
{
    return _terminal.get_size();
}

void Renderer::clear_particle_ids()
{
    _terminal.clear_particle_ids();
}
