#include "terminal.h"
#include "terminal.h"

#include <algorithm>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

static glm::ivec2 query_terminal_size_vec()
{
    glm::ivec2 size(0, 0);

#if defined(_WIN32)
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return size;
    }

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(handle, &info))
    {
        return size;
    }

    size.x = static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1);
    size.y = static_cast<int>(info.srWindow.Bottom - info.srWindow.Top + 1);
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
    {
        size.x = static_cast<int>(w.ws_col);
        size.y = static_cast<int>(w.ws_row);
    }
#endif

    return size;
}

static bool decode_utf8_first(const std::string& value, char32_t& out)
{
    if (value.empty())
    {
        return false;
    }

    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(value.data());
    unsigned char c = bytes[0];
    if (c < 0x80)
    {
        out = static_cast<char32_t>(c);
        return true;
    }

    if ((c >> 5) == 0x6 && value.size() >= 2)
    {
        unsigned char c1 = bytes[1];
        if ((c1 & 0xC0) != 0x80)
        {
            return false;
        }
        out = static_cast<char32_t>(((c & 0x1F) << 6) | (c1 & 0x3F));
        return true;
    }

    if ((c >> 4) == 0xE && value.size() >= 3)
    {
        unsigned char c1 = bytes[1];
        unsigned char c2 = bytes[2];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80)
        {
            return false;
        }
        out = static_cast<char32_t>(((c & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F));
        return true;
    }

    if ((c >> 3) == 0x1E && value.size() >= 4)
    {
        unsigned char c1 = bytes[1];
        unsigned char c2 = bytes[2];
        unsigned char c3 = bytes[3];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80)
        {
            return false;
        }
        out = static_cast<char32_t>(((c & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F));
        return true;
    }

    return false;
}

static bool vec3_equal(const glm::vec3& a, const glm::vec3& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

static std::string encode_utf8_char(char32_t value)
{
    std::string output;
    if (value <= 0x7F)
    {
        output.push_back(static_cast<char>(value));
        return output;
    }
    if (value <= 0x7FF)
    {
        output.push_back(static_cast<char>(0xC0 | ((value >> 6) & 0x1F)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        return output;
    }
    if (value <= 0xFFFF)
    {
        output.push_back(static_cast<char>(0xE0 | ((value >> 12) & 0x0F)));
        output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
        return output;
    }
    output.push_back(static_cast<char>(0xF0 | ((value >> 18) & 0x07)));
    output.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
    return output;
}

static int to_ansi_channel(float value, float max_component)
{
    float scaled = (max_component <= 1.0f) ? (value * 255.0f) : value;
    int channel = static_cast<int>(scaled + 0.5f);
    if (channel < 0)
    {
        return 0;
    }
    if (channel > 255)
    {
        return 255;
    }
    return channel;
}

TerminalSize get_terminal_size()
{
    glm::ivec2 size = query_terminal_size_vec();
    TerminalSize result{size.x, size.y};
    return result;
}

Terminal::Terminal()
{
    on_terminal_resize();
}

Terminal& Terminal::instance()
{
    static Terminal terminal;
    return terminal;
}

Terminal::Character::Character()
    : _glyph(U' '),
      dirty(true)
{
}

Terminal::Character::Character(const std::string& value, const glm::vec3& foreground, const glm::vec3& background)
    : _glyph(U' '),
      _foreground_colour(foreground),
      _background_colour(background)
{
    char32_t decoded = U' ';
    if (decode_utf8_first(value, decoded))
    {
        set_glyph(decoded);
    }
}

void Terminal::Character::set_glyph(char32_t glyph)
{
    _glyph = glyph;
    dirty = true;
}

char32_t Terminal::Character::get_glyph() const
{
    return _glyph;
}

void Terminal::Character::set_foreground_colour(const glm::vec3& colour)
{
    _foreground_colour = colour;
}

void Terminal::Character::set_background_colour(const glm::vec3& colour)
{
    _background_colour = colour;
}

void Terminal::Character::set_inverted(bool inverted)
{
    _is_inverted = inverted;
}

const glm::vec3& Terminal::Character::get_foreground_colour() const
{
    return _is_inverted ? _background_colour : _foreground_colour;
}

const glm::vec3& Terminal::Character::get_background_colour() const
{
    return _is_inverted ? _foreground_colour : _background_colour;
}

bool Terminal::Character::is_inverted() const
{
    return _is_inverted;
}

void Terminal::on_terminal_resize()
{
    glm::ivec2 size = query_terminal_size_vec();
    if (size != _size)
    {
        _size = size;
        _buffer.assign(static_cast<size_t>(_size.x * _size.y), Character{});
        _actual_state.assign(static_cast<size_t>(_size.x * _size.y), Character{});
        _has_current_colours = false;
    }
}

glm::ivec2 Terminal::get_size() const
{
    return _size;
}

const std::vector<Terminal::Character>& Terminal::get_buffer() const
{
    return _buffer;
}

std::vector<Terminal::Character>& Terminal::mutate_buffer()
{
    return _buffer;
}

void Terminal::scan_and_write_characters()
{
    if (_size.x <= 0)
    {
        return;
    }

    int width = _size.x;
    for (size_t index = 0; index < _buffer.size(); ++index)
    {
        if (!_buffer[index].dirty)
        {
            continue;
        }

        int y = static_cast<int>(index / static_cast<size_t>(width));
        int x = static_cast<int>(index % static_cast<size_t>(width));
        write_character_to_terminal(glm::ivec2(x, y));
    }
}

void Terminal::write_region(const glm::ivec2& min, const glm::ivec2& max)
{
    if (_size.x <= 0 || _size.y <= 0 || _buffer.empty())
    {
        return;
    }

    int min_x = std::max(0, min.x);
    int min_y = std::max(0, min.y);
    int max_x = std::min(_size.x - 1, max.x);
    int max_y = std::min(_size.y - 1, max.y);

    if (min_x > max_x || min_y > max_y)
    {
        return;
    }

    size_t length = static_cast<size_t>(max_x - min_x + 1);
    for (int y = min_y; y <= max_y; ++y)
    {
        write_string_to_terminal(glm::ivec2(min_x, y), length);
    }
}

void Terminal::write_character_to_terminal(const glm::ivec2& location)
{
    if (location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
    {
        return;
    }

    size_t index = static_cast<size_t>(location.y * _size.x + location.x);
    Character& desired = _buffer[index];
    const Character& actual = _actual_state[index];

    if (desired.get_glyph() == actual.get_glyph()
        && vec3_equal(desired.get_foreground_colour(), actual.get_foreground_colour())
        && vec3_equal(desired.get_background_colour(), actual.get_background_colour()))
    {
        return;
    }

    glm::vec3 desired_foreground = desired.get_foreground_colour();
    glm::vec3 desired_background = desired.get_background_colour();

    float fg_max = desired_foreground.x;
    if (desired_foreground.y > fg_max)
    {
        fg_max = desired_foreground.y;
    }
    if (desired_foreground.z > fg_max)
    {
        fg_max = desired_foreground.z;
    }

    float bg_max = desired_background.x;
    if (desired_background.y > bg_max)
    {
        bg_max = desired_background.y;
    }
    if (desired_background.z > bg_max)
    {
        bg_max = desired_background.z;
    }

    std::string sequence;
    sequence.reserve(64);
    sequence += "\x1b[" + std::to_string(location.y + 1) + ";" + std::to_string(location.x + 1) + "H";

    if (!_has_current_colours || !vec3_equal(desired_foreground, _current_foreground_colour))
    {
        int r = to_ansi_channel(desired_foreground.x, fg_max);
        int g = to_ansi_channel(desired_foreground.y, fg_max);
        int b = to_ansi_channel(desired_foreground.z, fg_max);
        sequence += "\x1b[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
        _current_foreground_colour = desired_foreground;
        _has_current_colours = true;
    }

    if (!_has_current_colours || !vec3_equal(desired_background, _current_background_colour))
    {
        int r = to_ansi_channel(desired_background.x, bg_max);
        int g = to_ansi_channel(desired_background.y, bg_max);
        int b = to_ansi_channel(desired_background.z, bg_max);
        sequence += "\x1b[48;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
        _current_background_colour = desired_background;
        _has_current_colours = true;
    }

    sequence += encode_utf8_char(desired.get_glyph());
    _actual_state[index] = desired;
    desired.dirty = false;

    std::cout << sequence;
    std::cout.flush();
}

void Terminal::write_string_to_terminal(const glm::ivec2& location, std::size_t length)
{
    if (length == 0 || location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
    {
        return;
    }

    if (_actual_state.size() != _buffer.size())
    {
        _actual_state.assign(_buffer.size(), Character{});
        _has_current_colours = false;
    }

    int width = _size.x;
    size_t start_index = static_cast<size_t>(location.y * width + location.x);
    size_t remaining_in_row = static_cast<size_t>(width - location.x);
    size_t max_count = remaining_in_row;
    if (start_index + max_count > _buffer.size())
    {
        max_count = _buffer.size() - start_index;
    }

    size_t count = length < max_count ? length : max_count;
    if (count == 0)
    {
        return;
    }

    bool any_dirty = false;
    for (size_t offset = 0; offset < count; ++offset)
    {
        const Character& desired = _buffer[start_index + offset];
        const Character& actual = _actual_state[start_index + offset];
        if (desired.get_glyph() != actual.get_glyph()
            || !vec3_equal(desired.get_foreground_colour(), actual.get_foreground_colour())
            || !vec3_equal(desired.get_background_colour(), actual.get_background_colour()))
        {
            any_dirty = true;
            break;
        }
    }

    if (!any_dirty)
    {
        return;
    }

    std::string sequence;
    sequence.reserve(64 + count * 12);

    for (size_t offset = 0; offset < count; ++offset)
    {
        size_t index = start_index + offset;
        Character& desired = _buffer[index];
        const Character& actual = _actual_state[index];

        if (desired.get_glyph() == actual.get_glyph()
            && vec3_equal(desired.get_foreground_colour(), actual.get_foreground_colour())
            && vec3_equal(desired.get_background_colour(), actual.get_background_colour()))
        {
            continue;
        }

        glm::vec3 desired_foreground = desired.get_foreground_colour();
        glm::vec3 desired_background = desired.get_background_colour();

        float fg_max = desired_foreground.x;
        if (desired_foreground.y > fg_max)
        {
            fg_max = desired_foreground.y;
        }
        if (desired_foreground.z > fg_max)
        {
            fg_max = desired_foreground.z;
        }

        float bg_max = desired_background.x;
        if (desired_background.y > bg_max)
        {
            bg_max = desired_background.y;
        }
        if (desired_background.z > bg_max)
        {
            bg_max = desired_background.z;
        }

        int x = location.x + static_cast<int>(offset);
        int y = location.y;
        sequence += "\x1b[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";

        if (!_has_current_colours || !vec3_equal(desired_foreground, _current_foreground_colour))
        {
            int r = to_ansi_channel(desired_foreground.x, fg_max);
            int g = to_ansi_channel(desired_foreground.y, fg_max);
            int b = to_ansi_channel(desired_foreground.z, fg_max);
            sequence += "\x1b[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
            _current_foreground_colour = desired_foreground;
            _has_current_colours = true;
        }

        if (!_has_current_colours || !vec3_equal(desired_background, _current_background_colour))
        {
            int r = to_ansi_channel(desired_background.x, bg_max);
            int g = to_ansi_channel(desired_background.y, bg_max);
            int b = to_ansi_channel(desired_background.z, bg_max);
            sequence += "\x1b[48;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
            _current_background_colour = desired_background;
            _has_current_colours = true;
        }

        sequence += encode_utf8_char(desired.get_glyph());
        _actual_state[index] = desired;
        desired.dirty = false;
    }

    if (!sequence.empty())
    {
        std::cout << sequence;
        std::cout.flush();
    }
}

glm::ivec2 Terminal::get_terminal_size() const
{
    return query_terminal_size_vec();
}

glm::ivec2& Terminal::mutate_size()
{
    return _size;
}
