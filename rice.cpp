#include "rice.h"

#include "app.h"
#include "draw.h"
#include "spdlog/spdlog.h"

#include <algorithm>

Rice::Rice() = default;

void Rice::run(const app_config& config)
{
    spdlog::trace("Rice::run()");

    auto renderer = Renderer::get();
    if (!renderer)
    {
        return;
    }

    std::vector<std::string> lines = config.rice_art;
    if (lines.empty())
    {
        lines.push_back("AGMP");
    }

    auto count_codepoints = [&](const std::string& text)
    {
        size_t index = 0;
        int count = 0;
        char32_t value = 0;
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

        while (decode_utf8(text, index, value))
        {
            count += 1;
        }
        return count;
    };

    int art_height = static_cast<int>(lines.size());
    int art_width = 0;
    for (const std::string& line : lines)
    {
        art_width = std::max(art_width, count_codepoints(line));
    }

    this->set_size(glm::ivec2(art_width, art_height));
    this->set_location(glm::ivec2(config.rice_origin_x, config.rice_origin_y));
    _buffer.assign(static_cast<size_t>(art_width * art_height), Terminal::Character{});

    glm::vec4 colour = config.rice_colour;
    for (int y = 0; y < art_height; ++y)
    {
        const std::string& line = lines[static_cast<size_t>(y)];
        size_t index = 0;
        int x = 0;
        char32_t glyph = 0;
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

        while (decode_utf8(line, index, glyph))
        {
            if (glyph != U' ')
            {
                int target_x = x;
                int target_y = y;
                if (target_x >= 0 && target_y >= 0 && target_x < art_width && target_y < art_height)
                {
                    size_t buffer_index = static_cast<size_t>(target_y * art_width + target_x);
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

    if (config.rice_draw_art)
    {
        renderer->set_overlay(_buffer, get_location(), get_size());
    }
    else
    {
        renderer->clear_overlay();
    }
}

const std::vector<Terminal::Character>& Rice::get_buffer() const
{
    return _buffer;
}
