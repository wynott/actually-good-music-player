#include "rice.h"

#include "rice.h"

#include "app.h"
#include "canvas.h"
#include "draw.h"
#include "spdlog/spdlog.h"
#include "terminal.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

Rice::Rice() = default;

void Rice::run(const app_config& config)
{

    load_art(config);
    _art_colour = glm::vec4(
        static_cast<float>(config.rice_colour.r),
        static_cast<float>(config.rice_colour.g),
        static_cast<float>(config.rice_colour.b),
        1.0f);
    if (auto renderer = Renderer::get())
    {
        Canvas* canvas = ActuallyGoodMP::instance().get_canvas();
        canvas->fill_gradient(
            glm::vec4(config.rice_background_tl.r, config.rice_background_tl.g, config.rice_background_tl.b, 1.0f),
            glm::vec4(config.rice_background_tr.r, config.rice_background_tr.g, config.rice_background_tr.b, 1.0f),
            glm::vec4(config.rice_background_bl.r, config.rice_background_bl.g, config.rice_background_bl.b, 1.0f),
            glm::vec4(config.rice_background_br.r, config.rice_background_br.g, config.rice_background_br.b, 1.0f));
        renderer->set_canvas(canvas->get_buffer());
    }

    for (int frame = 0; frame < 4; ++frame)
    {
        draw_frame(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }

    draw_frame(-1);
}

void Rice::draw_frame(int frame)
{
    spdlog::trace("Rice::draw_frame()");
    
    ensure_buffer();
    clear_buffer();

    auto decode_utf8 = [](const std::string& text, size_t& index, char32_t& out)
    {
        if (index >= text.size())
        {
            return false;
        }

        unsigned char c0 = static_cast<unsigned char>(text[index]);
        if ((c0 & 0x80) == 0)
        {
            out = static_cast<char32_t>(c0);
            index += 1;
            return true;
        }
        if ((c0 & 0xE0) == 0xC0 && index + 1 < text.size())
        {
            unsigned char c1 = static_cast<unsigned char>(text[index + 1]);
            if ((c1 & 0xC0) != 0x80)
            {
                index += 1;
                return false;
            }
            out = static_cast<char32_t>(((c0 & 0x1F) << 6) | (c1 & 0x3F));
            index += 2;
            return true;
        }
        if ((c0 & 0xF0) == 0xE0 && index + 2 < text.size())
        {
            unsigned char c1 = static_cast<unsigned char>(text[index + 1]);
            unsigned char c2 = static_cast<unsigned char>(text[index + 2]);
            if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80)
            {
                index += 1;
                return false;
            }
            out = static_cast<char32_t>(((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F));
            index += 3;
            return true;
        }
        if ((c0 & 0xF8) == 0xF0 && index + 3 < text.size())
        {
            unsigned char c1 = static_cast<unsigned char>(text[index + 1]);
            unsigned char c2 = static_cast<unsigned char>(text[index + 2]);
            unsigned char c3 = static_cast<unsigned char>(text[index + 3]);
            if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80)
            {
                index += 1;
                return false;
            }
            out = static_cast<char32_t>(((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F));
            index += 4;
            return true;
        }

        index += 1;
        return false;
    };

    auto count_codepoints = [&](const std::string& text)
    {
        size_t index = 0;
        size_t count = 0;
        char32_t value = 0;
        while (decode_utf8(text, index, value))
        {
            count += 1;
        }
        return static_cast<int>(count);
    };

    auto set_cell = [&](int x, int y, char32_t glyph)
    {
        if (x < 0 || y < 0 || x >= _size.x || y >= _size.y)
        {
            return;
        }
        size_t index = static_cast<size_t>(y * _size.x + x);
        if (index >= _buffer.size())
        {
            return;
        }
        Terminal::Character& cell = _buffer[index];
        cell.set_glyph(glyph);
        cell.set_glyph_colour(_art_colour);
        cell.set_background_colour(glm::vec4(0.0f));
    };

    int art_height = static_cast<int>(_lines.size());
    int art_width = 0;
    for (const std::string& line : _lines)
    {
        art_width = std::max(art_width, count_codepoints(line));
    }

    glm::ivec2 center = get_center();
    glm::ivec2 top_left(center.x - art_width / 2, center.y - art_height / 2);

    for (int y = 0; y < art_height; ++y)
    {
        const std::string& line = _lines[static_cast<size_t>(y)];
        size_t index = 0;
        int x = 0;
        char32_t glyph = 0;
        while (decode_utf8(line, index, glyph))
        {
            if (glyph != U' ')
            {
                set_cell(top_left.x + x, top_left.y + y, glyph);
            }
            x += 1;
        }
    }

    if (auto renderer = Renderer::get())
    {
        renderer->set_canvas(_buffer);
    }

    if (frame < 0)
    {
        return;
    }
}

glm::ivec2 Rice::get_center() const
{
    auto renderer = Renderer::get();
    if (!renderer)
    {
        return glm::ivec2(0);
    }
    glm::ivec2 size = renderer->get_terminal_size();
    return glm::ivec2(size.x / 2, size.y / 2);
}

void Rice::ensure_buffer()
{
    auto renderer = Renderer::get();
    if (!renderer)
    {
        return;
    }
    glm::ivec2 size = renderer->get_terminal_size();
    if (size != _size)
    {
        _size = size;
        _buffer.assign(static_cast<size_t>(_size.x * _size.y), Terminal::Character{});
    }
}

void Rice::clear_buffer()
{
    for (Terminal::Character& cell : _buffer)
    {
        cell.set_glyph(U' ');
        cell.set_glyph_colour(glm::vec4(0.0f));
        cell.set_background_colour(glm::vec4(0.0f));
    }
}

void Rice::load_art(const app_config& config)
{
    _lines = config.rice_art;
    if (_lines.empty())
    {
        _lines.push_back("AGMP");
    }
}
