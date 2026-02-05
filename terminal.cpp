#include "terminal.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <cstring>
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

void Terminal::init()
{
#if defined(_WIN32)
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE)
    {
        SetConsoleOutputCP(CP_UTF8);
        DWORD mode = 0;
        if (GetConsoleMode(handle, &mode))
        {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
            SetConsoleMode(handle, mode);
        }
    }
#endif
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

static bool vec4_equal(const glm::vec4& a, const glm::vec4& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

static glm::vec4 normalize_colour(const glm::vec4& colour)
{
    bool clamped = false;
    glm::vec4 result = colour;
    if (result.x < 0.0f)
    {
        result.x = 0.0f;
        clamped = true;
    }
    if (result.y < 0.0f)
    {
        result.y = 0.0f;
        clamped = true;
    }
    if (result.z < 0.0f)
    {
        result.z = 0.0f;
        clamped = true;
    }
    if (result.w < 0.0f)
    {
        result.w = 0.0f;
        clamped = true;
    }
    if (result.x > 1.0f)
    {
        result.x = 1.0f;
        clamped = true;
    }
    if (result.y > 1.0f)
    {
        result.y = 1.0f;
        clamped = true;
    }
    if (result.z > 1.0f)
    {
        result.z = 1.0f;
        clamped = true;
    }
    if (result.w > 1.0f)
    {
        result.w = 1.0f;
        clamped = true;
    }

    if (clamped)
    {
        std::exit(-1);
    }

    return result;
}

static bool is_empty_glyph(const Terminal::Character& value)
{
    static const glm::vec4 kDefaultColour(0.0f);
    if (value.get_glyph() != U' ')
    {
        return false;
    }
    if (value.get_glyph_colour() != kDefaultColour)
    {
        return false;
    }
    if (!vec4_equal(value.get_background_colour(), kDefaultColour))
    {
        return false;
    }
    return true;
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

static std::string to_ansi_channel(const glm::vec4& colour, bool background)
{
    auto clamp_unit = [](float value)
    {
        if (value < 0.0f)
        {
            value = 0.0f;
        }
        if (value > 1.0f)
        {
            value = 1.0f;
        }
        return static_cast<int>(value * 255.0f + 0.5f);
    };

    int r = clamp_unit(colour.x);
    int g = clamp_unit(colour.y);
    int b = clamp_unit(colour.z);
    std::string sequence;
    sequence.reserve(24);
    sequence += "\x1b[";
    sequence += background ? "48" : "38";
    sequence += ";2;";
    sequence += std::to_string(r);
    sequence += ";";
    sequence += std::to_string(g);
    sequence += ";";
    sequence += std::to_string(b);
    sequence += "m";
    return sequence;
}

Terminal::Terminal()
{
    std::cout << "\x1b[?25l";
    std::cout.flush();
    on_terminal_resize();
}

void Terminal::shutdown()
{
    clear_screen();
    std::fwrite("\x1b[0m\x1b[?25h", 1, 10, stdout);
    std::fflush(stdout);
}

Terminal::BackingStore::BackingStore() = default;

Terminal::BackingStore::BackingStore(int width, int height)
{
    resize(width, height);
}

void Terminal::BackingStore::resize(int width, int height)
{
    this->width = std::max(0, width);
    this->height = std::max(0, height);
    size_t count = static_cast<size_t>(this->width * this->height);
    buffer.assign(count, Character{});
    canvas.assign(count, Character{});
    dirty.assign(count, true);
}

Terminal::Character::Character()
    : _glyph(U' '),
      _glyph_colour(glm::vec4(0.0f))
{
}

Terminal::Character::Character(const std::string& value, const glm::vec4& foreground, const glm::vec4& background)
    : _glyph(U' '),
      _glyph_colour(foreground),
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
}

char32_t Terminal::Character::get_glyph() const
{
    return _glyph;
}

void Terminal::Character::set_glyph_colour(const glm::vec4& colour)
{
    _glyph_colour = colour;
}

void Terminal::Character::set_background_colour(const glm::vec4& colour)
{
    _background_colour = colour;
}

const glm::vec4& Terminal::Character::get_glyph_colour() const
{
    return _glyph_colour;
}

const glm::vec4& Terminal::Character::get_background_colour() const
{
    return _background_colour;
}

void Terminal::on_terminal_resize()
{
    glm::ivec2 size = query_terminal_size_vec();
    if (size != _size)
    {
        _size = size;
        _store.resize(_size.x, _size.y);
        _current_glyph_colour = glm::vec4(-1.0f);
        _current_background_colour = glm::vec4(-1.0f);
    }
}

glm::ivec2 Terminal::get_size() const
{
    if (_size.x <= 0 || _size.y <= 0)
    {
        spdlog::warn("Terminal size zero");
    }

    return _size;
}

glm::ivec2 Terminal::get_location(size_t buffer_index) const
{
    if (_size.x <= 0)
    {
        return glm::ivec2(0);
    }

    int width = _size.x;
    int y = static_cast<int>(buffer_index / static_cast<size_t>(width));
    int x = static_cast<int>(buffer_index % static_cast<size_t>(width));
    return glm::ivec2(x, y);
}

size_t Terminal::get_index(const glm::ivec2& location) const
{
    if (_size.x <= 0)
    {
        return 0;
    }

    return static_cast<size_t>(location.y * _size.x + location.x);
}

glm::vec4 Terminal::get_canvas_sample(const glm::ivec2& location) const
{
    if (location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
    {
        return glm::vec4(0.0f);
    }

    size_t index = get_index(location);
    if (index >= _store.canvas.size())
    {
        return glm::vec4(0.0f);
    }

    const Character& cell = _store.canvas[index];
    return cell.get_background_colour();
}

void Terminal::set_glyph(char32_t glyph, const glm::ivec2& location)
{
    if (location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
    {
        return;
    }

    size_t index = get_index(location);
    if (index >= _store.buffer.size())
    {
        return;
    }

    _store.buffer[index].set_glyph(glyph);
    _store.dirty[index] = true;
}

void Terminal::set_glyph(const glm::ivec2& location, char32_t glyph, const glm::vec4& foreground, const glm::vec4& background)
{
    if (location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
    {
        return;
    }

    size_t index = get_index(location);
    if (index >= _store.buffer.size())
    {
        return;
    }

    Terminal::Character& cell = _store.buffer[index];
    cell.set_glyph(glyph);
    glm::vec4 fg = normalize_colour(foreground);
    glm::vec4 bg = normalize_colour(background);
    cell.set_glyph_colour(fg);
    cell.set_background_colour(bg);
    _store.dirty[index] = true;
}

void Terminal::clear_cell(const glm::ivec2& location)
{
    if (location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
    {
        return;
    }

    size_t index = get_index(location);
    if (index >= _store.buffer.size())
    {
        return;
    }

    Terminal::Character& cell = _store.buffer[index];
    cell.set_glyph(U' ');
    cell.set_glyph_colour(glm::vec4(0.0f));
    cell.set_background_colour(glm::vec4(0.0f));
    _store.dirty[index] = true;
}

void Terminal::set_canvas(const std::vector<Character>& source)
{
    if (source.size() != _store.canvas.size())
    {
        return;
    }

    for (size_t i = 0; i < source.size(); ++i)
    {
        _store.canvas[i] = source[i];
        _store.dirty[i] = true;
    }
}

bool Terminal::is_dirty(const glm::ivec2& location) const
{
    if (location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
    {
        return false;
    }

    size_t index = get_index(location);
    if (index >= _store.dirty.size())
    {
        return false;
    }

    return _store.dirty[index];
}

void Terminal::update()
{
    on_terminal_resize();
    if (_size.x <= 0 || _size.y <= 0)
    {
        return;
    }

    size_t total = _store.dirty.size();
    if (total == 0)
    {
        return;
    }

    size_t index = 0;
    while (index < total)
    {
        if (!_store.dirty[index])
        {
            index += 1;
            continue;
        }

        glm::ivec2 location = get_location(index);
        if (location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
        {
            index += 1;
            continue;
        }

        size_t remaining_in_row = static_cast<size_t>(_size.x - location.x);
        size_t run = 0;
        while (run < remaining_in_row && index + run < total && _store.dirty[index + run])
        {
            run += 1;
        }

        if (run <= 1)
        {
            write_string_to_terminal(location, 1);
            index += 1;
            continue;
        }

        write_string_to_terminal(location, run);
        index += run;
    }
}

void Terminal::mark_all_dirty()
{
    if (_store.dirty.empty())
    {
        return;
    }

    std::fill(_store.dirty.begin(), _store.dirty.end(), true);
}

void Terminal::clear_screen()
{
#if defined(_WIN32)
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(handle, &info))
    {
        return;
    }

    DWORD cell_count = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
    DWORD written = 0;
    COORD home = {0, 0};
    WORD default_attrs = static_cast<WORD>(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    FillConsoleOutputCharacter(handle, ' ', cell_count, home, &written);
    FillConsoleOutputAttribute(handle, default_attrs, cell_count, home, &written);
    SetConsoleTextAttribute(handle, default_attrs);
    SetConsoleCursorPosition(handle, home);
#else
    const char* sequence = "\x1b[3J\x1b[2J\x1b[H\x1b[0m";
    std::fwrite(sequence, 1, std::strlen(sequence), stdout);
    std::fflush(stdout);
#endif
}

void Terminal::write_string_to_terminal(const glm::ivec2& location, std::size_t length)
{
    if (length == 0)
    {
        return;
    }
    if (location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
    {
        return;
    }

    if (_store.buffer.empty())
    {
        return;
    }

    size_t start_index = get_index(location);
    size_t remaining_in_row = static_cast<size_t>(_size.x - location.x);
    size_t max_count = remaining_in_row;
    if (start_index + max_count > _store.buffer.size())
    {
        max_count = _store.buffer.size() - start_index;
    }

    size_t count = max_count;
    if (length < max_count)
    {
        count = length;
    }
    if (count == 0)
    {
        return;
    }

    std::string sequence;
    sequence.reserve(64 + count * 12);

    for (size_t offset = 0; offset < count; ++offset)
    {
        size_t index = start_index + offset;
        Character& buffer_cell = _store.buffer[index];
        Character& canvas_cell = _store.canvas[index];
        const Character* desired = &buffer_cell;
        if (is_empty_glyph(buffer_cell))
        {
            desired = &canvas_cell;
        }
        glm::vec4 desired_foreground = desired->get_glyph_colour();
        glm::vec4 desired_background = desired->get_background_colour();
        if (desired_background.w == 0.0f)
        {
            desired_background = canvas_cell.get_background_colour();
        }

        glm::ivec2 cell_location = get_location(index);
        sequence += "\x1b[" + std::to_string(cell_location.y + 1) + ";" + std::to_string(cell_location.x + 1) + "H";

        if (!vec4_equal(desired_foreground, _current_glyph_colour))
        {
            sequence += to_ansi_channel(desired_foreground, false);
            _current_glyph_colour = desired_foreground;
        }

        if (!vec4_equal(desired_background, _current_background_colour))
        {
            sequence += to_ansi_channel(desired_background, true);
            _current_background_colour = desired_background;
        }

        sequence += encode_utf8_char(desired->get_glyph());
        _store.dirty[index] = false;
    }

    if (!sequence.empty())
    {
        // std::cout << sequence;
        // std::cout.flush();
        std::fwrite(sequence.data(), 1, sequence.size(), stdout);
        std::fflush(stdout);
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
