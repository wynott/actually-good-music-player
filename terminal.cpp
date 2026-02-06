#include "terminal.h"
#include "terminal.h"

#include "app.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdint>
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

    _store.resize(_size.x, _size.y);
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


static bool u8vec3_equal(const glm::u8vec3& a, const glm::u8vec3& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

static std::string to_ansi_channel(const glm::u8vec3& colour, bool background)
{
    std::string sequence;
    sequence.reserve(24);
    sequence += "\x1b[";
    sequence += background ? "48" : "38";
    sequence += ";2;";
    sequence += std::to_string(colour.r);
    sequence += ";";
    sequence += std::to_string(colour.g);
    sequence += ";";
    sequence += std::to_string(colour.b);
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

Terminal::BackingStore::BackingStore()
    : layers("terminal.layers")
{
    layer_order = {"wallpaper", "logo", "buffer", "juice"};
}

Terminal::BackingStore::BackingStore(int width, int height)
    : BackingStore()
{
    resize(width, height);
}

void Terminal::BackingStore::resize(int width, int height)
{
    this->width = std::max(0, width);
    this->height = std::max(0, height);
    size_t count = static_cast<size_t>(this->width * this->height);
    for (const auto& name : layer_order)
    {
        auto& layer_ref = layers.get_or_emplace(name);
        layer_ref.assign(count, Character{});
    }
    this->pending_frame.assign(count, Character8{});
    this->previous_frame.assign(count, Character8{});
    dirty.assign(count, true);
}

std::vector<Terminal::Character>& Terminal::BackingStore::layer(const std::string& name)
{
    return layers.at(name);
}

const std::vector<Terminal::Character>& Terminal::BackingStore::layer(const std::string& name) const
{
    return layers.at(name);
}

bool Terminal::BackingStore::has_layer(const std::string& name) const
{
    return layers.contains(name);
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

Terminal::Character8::Character8()
    : _glyph(U' '),
      _glyph_colour(glm::u8vec3(0)),
      _background_colour(glm::u8vec3(0)),
      _particle_id(0)
{
}

Terminal::Character8::Character8(const std::string& value, const glm::u8vec3& foreground, const glm::u8vec3& background)
    : _glyph(U' '),
      _glyph_colour(foreground),
      _background_colour(background),
      _particle_id(0)
{
    char32_t decoded = U' ';
    if (decode_utf8_first(value, decoded))
    {
        set_glyph(decoded);
    }
}

void Terminal::Character8::set_glyph(char32_t glyph)
{
    _glyph = glyph;
}

char32_t Terminal::Character8::get_glyph() const
{
    return _glyph;
}

void Terminal::Character8::set_glyph_colour(const glm::u8vec3& colour)
{
    _glyph_colour = colour;
}

void Terminal::Character8::set_background_colour(const glm::u8vec3& colour)
{
    _background_colour = colour;
}


const glm::u8vec3& Terminal::Character8::get_glyph_colour() const
{
    return _glyph_colour;
}

const glm::u8vec3& Terminal::Character8::get_background_colour() const
{
    return _background_colour;
}

void Terminal::Character8::set_particle_id(uint32_t particle_id)
{
    _particle_id = particle_id;
}

uint32_t Terminal::Character8::get_particle_id() const
{
    return _particle_id;
}

bool Terminal::Character8::operator==(const Character8& other) const
{
    return _glyph == other._glyph
        && u8vec3_equal(_glyph_colour, other._glyph_colour)
        && u8vec3_equal(_background_colour, other._background_colour)
        && _particle_id == other._particle_id;
}

bool Terminal::Character8::operator!=(const Character8& other) const
{
    return !(*this == other);
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
    const auto& canvas_layer = _store.layer("wallpaper");
    if (index >= canvas_layer.size())
    {
        return glm::vec4(0.0f);
    }

    const Character& cell = canvas_layer[index];
    return cell.get_background_colour();
}

void Terminal::set_glyph(char32_t glyph, const glm::ivec2& location)
{
    if (location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
    {
        return;
    }

    size_t index = get_index(location);
    auto& buffer_layer = _store.layer("buffer");
    if (index >= buffer_layer.size())
    {
        return;
    }

    buffer_layer[index].set_glyph(glyph);
    _store.pending_frame[index].set_particle_id(0u);
    _store.dirty[index] = true;
}

void Terminal::set_glyph(const glm::ivec2& location, char32_t glyph, const glm::vec4& foreground, const glm::vec4& background)
{
    if (location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
    {
        return;
    }

    size_t index = get_index(location);
    auto& buffer_layer = _store.layer("buffer");
    if (index >= buffer_layer.size())
    {
        return;
    }

    Terminal::Character& cell = buffer_layer[index];
    cell.set_glyph(glyph);
    glm::vec4 fg = normalize_colour(foreground);
    glm::vec4 bg = normalize_colour(background);
    cell.set_glyph_colour(fg);
    cell.set_background_colour(bg);
    _store.pending_frame[index].set_particle_id(0u);
    _store.dirty[index] = true;
}

void Terminal::set_particle_glyph(const glm::ivec2& location, char32_t glyph, const glm::vec4& foreground, const glm::vec4& background, uint32_t particle_id)
{
    if (location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
    {
        return;
    }

    size_t index = get_index(location);
    auto& juice_layer = _store.layer("juice");
    if (index >= juice_layer.size())
    {
        return;
    }

    Terminal::Character& cell = juice_layer[index];
    cell.set_glyph(glyph);
    glm::vec4 fg = normalize_colour(foreground);
    glm::vec4 bg = normalize_colour(background);
    cell.set_glyph_colour(fg);
    cell.set_background_colour(bg);
    _store.pending_frame[index].set_particle_id(particle_id);
    _store.dirty[index] = true;
}

void Terminal::clear_cell(const glm::ivec2& location)
{
    if (location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
    {
        return;
    }

    size_t index = get_index(location);
    auto& buffer_layer = _store.layer("buffer");
    if (index >= buffer_layer.size())
    {
        return;
    }

    Terminal::Character& cell = buffer_layer[index];
    cell.set_glyph(U' ');
    cell.set_glyph_colour(glm::vec4(0.0f));
    cell.set_background_colour(glm::vec4(0.0f));
    _store.pending_frame[index].set_particle_id(0u);
    _store.dirty[index] = true;
}

void Terminal::clear_juice()
{
    auto& juice_layer = _store.layer("juice");
    if (juice_layer.empty())
    {
        return;
    }

    for (size_t index = 0; index < juice_layer.size(); ++index)
    {
        if (!is_empty_glyph(juice_layer[index]))
        {
            juice_layer[index] = Character{};
            _store.pending_frame[index].set_particle_id(0u);
            _store.dirty[index] = true;
        }
    }
}

void Terminal::set_canvas(const std::vector<Character>& source)
{
    set_layer("wallpaper", source);
}

void Terminal::set_layer(const std::string& name, const std::vector<Character>& source)
{
    if (!_store.has_layer(name))
    {
        return;
    }

    auto& layer_ref = _store.layer(name);
    if (source.size() != layer_ref.size())
    {
        return;
    }

    for (size_t i = 0; i < source.size(); ++i)
    {
        layer_ref[i] = source[i];
        _store.dirty[i] = true;
    }
}

void Terminal::select_region(const glm::ivec2& location, const glm::ivec2& size)
{
    if (size.x <= 0 || size.y <= 0)
    {
        return;
    }

    int min_x = std::max(0, location.x);
    int min_y = std::max(0, location.y);
    int max_x = std::min(_size.x - 1, location.x + size.x - 1);
    int max_y = std::min(_size.y - 1, location.y + size.y - 1);
    if (min_x > max_x || min_y > max_y)
    {
        return;
    }

    glm::vec4 highlight(0.2f, 0.2f, 0.2f, 1.0f);
    for (int y = min_y; y <= max_y; ++y)
    {
        for (int x = min_x; x <= max_x; ++x)
        {
            size_t index = get_index(glm::ivec2(x, y));
            auto& buffer_layer = _store.layer("buffer");
            if (index >= buffer_layer.size())
            {
                continue;
            }
            buffer_layer[index].set_background_colour(highlight);
            _store.dirty[index] = true;
        }
    }
}

void Terminal::deselect_region(const glm::ivec2& location, const glm::ivec2& size)
{
    if (size.x <= 0 || size.y <= 0)
    {
        return;
    }

    int min_x = std::max(0, location.x);
    int min_y = std::max(0, location.y);
    int max_x = std::min(_size.x - 1, location.x + size.x - 1);
    int max_y = std::min(_size.y - 1, location.y + size.y - 1);
    if (min_x > max_x || min_y > max_y)
    {
        return;
    }

    glm::vec4 clear_bg(0.0f);
    for (int y = min_y; y <= max_y; ++y)
    {
        for (int x = min_x; x <= max_x; ++x)
        {
            size_t index = get_index(glm::ivec2(x, y));
            auto& buffer_layer = _store.layer("buffer");
            if (index >= buffer_layer.size())
            {
                continue;
            }
            buffer_layer[index].set_background_colour(clear_bg);
            _store.dirty[index] = true;
        }
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

    eightbitify();
    update_eightbit();
}


void Terminal::eightbitify()
{
    if (_store.layer("buffer").empty() || _store.dirty.empty())
    {
        return;
    }


    auto to_u8 = [](float value)
    {
        value = std::clamp(value, 0.0f, 1.0f);
        return static_cast<uint8_t>(value * 255.0f + 0.5f);
    };

    auto& buffer_layer = _store.layer("buffer");
    auto& juice_layer = _store.layer("juice");
    const auto& wallpaper_layer = _store.layer("wallpaper");
    const auto& logo_layer = _store.layer("logo");
    for (size_t index = 0; index < buffer_layer.size(); ++index)
    {
        if (!_store.dirty[index])
        {
            continue;
        }

        const Character& buffer_cell = buffer_layer[index];
        const Character& juice_cell = juice_layer[index];
        const Character& logo_cell = logo_layer[index];
        const Character* wallpaper_cell = &wallpaper_layer[index];
        const Character* desired = &buffer_cell;

        if (is_empty_glyph(buffer_cell))
        {
            if (!is_empty_glyph(logo_cell))
            {
                desired = &logo_cell;
            }
            else
            {
                desired = wallpaper_cell;
            }
        }

        const Character* overlay = nullptr;
        if (!is_empty_glyph(juice_cell))
        {
            overlay = &juice_cell;
        }


        glm::vec4 fg = desired->get_glyph_colour();
        glm::vec4 bg = desired->get_background_colour();

        if (desired == &logo_cell)
        {
            bg = wallpaper_cell->get_background_colour();
        }

        if (desired == &buffer_cell && bg.w < 1.0f)
        {
            glm::vec4 canvas_bg = wallpaper_cell->get_background_colour();
            float inv = 1.0f - bg.w;
            bg = glm::vec4(
                bg.r * bg.w + canvas_bg.r * inv,
                bg.g * bg.w + canvas_bg.g * inv,
                bg.b * bg.w + canvas_bg.b * inv,
                1.0f);
        }

        if (overlay)
        {
            glm::vec4 overlay_fg = overlay->get_glyph_colour();
            glm::vec4 overlay_bg = overlay->get_background_colour();
            if (overlay_bg.w < 1.0f)
            {
                float inv = 1.0f - overlay_bg.w;
                overlay_bg = glm::vec4(
                    overlay_bg.r * overlay_bg.w + bg.r * inv,
                    overlay_bg.g * overlay_bg.w + bg.g * inv,
                    overlay_bg.b * overlay_bg.w + bg.b * inv,
                    1.0f);
            }
            fg = overlay_fg;
            bg = overlay_bg;
            desired = overlay;
        }

        Character8& out = _store.pending_frame[index];
        out.set_glyph(desired->get_glyph());
        out.set_glyph_colour(glm::u8vec3(to_u8(fg.r), to_u8(fg.g), to_u8(fg.b)));
        out.set_background_colour(glm::u8vec3(to_u8(bg.r), to_u8(bg.g), to_u8(bg.b)));

        _store.dirty[index] = false;
    }
}

void Terminal::update_eightbit()
{
    on_terminal_resize();
    if (_size.x <= 0 || _size.y <= 0)
    {
        return;
    }

    size_t total = _store.pending_frame.size();
    if (total == 0
        || _store.previous_frame.size() != total)
    {
        return;
    }

    std::string sequence;
    sequence.reserve(64 + total * 6);

    bool have_fg = false;
    bool have_bg = false;
    glm::u8vec3 current_fg(0);
    glm::u8vec3 current_bg(0);

    for (size_t index = 0; index < total; ++index)
    {
        const Character8& next = _store.pending_frame[index];
        const Character8& prev = _store.previous_frame[index];
        if (next == prev)
        {
            continue;
        }

        glm::ivec2 location = get_location(index);
        if (location.x < 0 || location.y < 0 || location.x >= _size.x || location.y >= _size.y)
        {
            continue;
        }

        sequence += "\x1b[" + std::to_string(location.y + 1) + ";" + std::to_string(location.x + 1) + "H";

        const glm::u8vec3& fg = next.get_glyph_colour();
        const glm::u8vec3& bg = next.get_background_colour();

        if (!have_fg || !u8vec3_equal(fg, current_fg))
        {
            sequence += to_ansi_channel(fg, false);
            current_fg = fg;
            have_fg = true;
        }

        if (!have_bg || !u8vec3_equal(bg, current_bg))
        {
            sequence += to_ansi_channel(bg, true);
            current_bg = bg;
            have_bg = true;
        }

        sequence += encode_utf8_char(next.get_glyph());
        _store.previous_frame[index] = next;
    }

    if (!sequence.empty())
    {
        std::fwrite(sequence.data(), 1, sequence.size(), stdout);
        std::fflush(stdout);
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


glm::ivec2 Terminal::get_terminal_size() const
{
    return query_terminal_size_vec();
}

glm::ivec2& Terminal::mutate_size()
{
    return _size;
}
