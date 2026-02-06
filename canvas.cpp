#include "canvas.h"

#include "canvas.h"

#include "album_art.h"

#include <algorithm>
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

void Canvas::build_default(const app_config& config)
{
    if (_size.x <= 0 || _size.y <= 0)
    {
        return;
    }

    fill_gradient(
        config.rice_background_tl,
        config.rice_background_tr,
        config.rice_background_bl,
        config.rice_background_br);

    if (config.rice_draw_art)
    {
        std::vector<std::string> lines = config.rice_art;
        if (lines.empty())
        {
            lines.push_back("AGMP");
        }
        draw_art(lines, config.rice_colour);
    }
}

void Canvas::build_from_album(const app_config& config, const AlbumArt& album_art)
{
    glm::vec4 avg_tl(0.0f);
    glm::vec4 avg_tr(0.0f);
    glm::vec4 avg_bl(0.0f);
    glm::vec4 avg_br(0.0f);
    if (album_art.get_quadrant_colours(avg_tl, avg_tr, avg_bl, avg_br))
    {
        fill_gradient(avg_tl, avg_tr, avg_bl, avg_br);
        if (config.rice_draw_art)
        {
            std::vector<std::string> lines = config.rice_art;
            if (lines.empty())
            {
                lines.push_back("AGMP");
            }
            draw_art(lines, config.rice_colour);
        }
        return;
    }

    build_default(config);
}

void Canvas::build_grid(const app_config& config)
{
    if (_size.x <= 0 || _size.y <= 0)
    {
        return;
    }

    fill_gradient(
        config.grid_background_tl,
        config.grid_background_tr,
        config.grid_background_bl,
        config.grid_background_br);

    int spacing_x = std::max(1, config.grid_spacing_x);
    int spacing_y = std::max(1, config.grid_spacing_y);
    glm::vec4 line_colour = config.grid_line_colour;

    for (int y = 0; y < _size.y; ++y)
    {
        int from_bottom = (_size.y - 1) - y;
        bool on_h = (from_bottom % spacing_y) == 0;
        for (int x = 0; x < _size.x; ++x)
        {
            bool on_v = (x % spacing_x) == 0;
            if (!on_h && !on_v)
            {
                continue;
            }

            char32_t glyph = on_h && on_v ? U'┼' : (on_h ? U'─' : U'│');
            size_t index = static_cast<size_t>(y * _size.x + x);
            if (index >= _buffer.size())
            {
                continue;
            }

            Terminal::Character& cell = _buffer[index];
            cell.set_glyph(glyph);
            cell.set_glyph_colour(line_colour);
        }
    }

    auto write_label = [&](int x, int y, const std::string& text)
    {
        if (y < 0 || y >= _size.y)
        {
            return;
        }
        for (size_t i = 0; i < text.size(); ++i)
        {
            int target_x = x + static_cast<int>(i);
            if (target_x < 0 || target_x >= _size.x)
            {
                break;
            }
            size_t index = static_cast<size_t>(y * _size.x + target_x);
            if (index >= _buffer.size())
            {
                break;
            }
            Terminal::Character& cell = _buffer[index];
            cell.set_glyph(static_cast<char32_t>(text[i]));
            cell.set_glyph_colour(line_colour);
        }
    };

    int label_every = std::max(1, config.grid_label_every);
    int top_y = 0;
    int bottom_y = std::max(0, _size.y - 1);
    int left_x = 0;
    int right_x = std::max(0, _size.x - 1);

    for (int x = 0; x < _size.x; x += spacing_x)
    {
        int line_index = x / spacing_x;
        if (line_index % label_every != 0)
        {
            continue;
        }
        std::string label = std::to_string(x);
        write_label(x, top_y, label);
        write_label(x, bottom_y, label);
    }

    for (int y = 0; y < _size.y; ++y)
    {
        int from_bottom = (_size.y - 1) - y;
        if (from_bottom % spacing_y != 0)
        {
            continue;
        }
        int line_index = from_bottom / spacing_y;
        if (line_index % label_every != 0)
        {
            continue;
        }
        std::string label = std::to_string(from_bottom);
        write_label(left_x, y, label);
        write_label(std::max(0, right_x - static_cast<int>(label.size()) + 1), y, label);
    }

    write_label(left_x, top_y, std::to_string(std::max(0, _size.y - 1)));

    if (config.rice_draw_art)
    {
        std::vector<std::string> lines = config.rice_art;
        if (lines.empty())
        {
            lines.push_back("AGMP");
        }
        draw_art(lines, config.rice_colour);
    }
}

static bool decode_utf8(const std::string& text, size_t& index, char32_t& out)
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
}

static int count_codepoints(const std::string& text)
{
    size_t index = 0;
    int count = 0;
    char32_t value = 0;
    while (decode_utf8(text, index, value))
    {
        count += 1;
    }
    return count;
}

void Canvas::draw_art(const std::vector<std::string>& lines, const glm::vec4& colour)
{
    if (_size.x <= 0 || _size.y <= 0 || lines.empty())
    {
        return;
    }

    int art_height = static_cast<int>(lines.size());
    int art_width = 0;
    for (const std::string& line : lines)
    {
        art_width = std::max(art_width, count_codepoints(line));
    }

    glm::ivec2 center(_size.x / 2, _size.y / 2);
    glm::ivec2 top_left(center.x - art_width / 2, center.y - art_height / 2);

    for (int y = 0; y < art_height; ++y)
    {
        const std::string& line = lines[static_cast<size_t>(y)];
        size_t index = 0;
        int x = 0;
        char32_t glyph = 0;
        while (decode_utf8(line, index, glyph))
        {
            if (glyph != U' ')
            {
                int target_x = top_left.x + x;
                int target_y = top_left.y + y;
                if (target_x >= 0 && target_y >= 0 && target_x < _size.x && target_y < _size.y)
                {
                    size_t buffer_index = static_cast<size_t>(target_y * _size.x + target_x);
                    if (buffer_index < _buffer.size())
                    {
                        Terminal::Character& cell = _buffer[buffer_index];
                        cell.set_glyph(glyph);
                        cell.set_glyph_colour(colour);
                    }
                }
            }
            x += 1;
        }
    }
}
