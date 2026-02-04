#include "draw.h"

#include <algorithm>
#include <iostream>

#include "draw.h"
#include "terminal.h"

void draw_frame()
{
    TerminalSize size = get_terminal_size();
    if (size.columns <= 0 || size.rows <= 0)
    {
        return;
    }

    int columns = size.columns;
    int rows = size.rows;
    int right_col = (columns > 1) ? columns - 1 : 1;
    int bottom_row = (rows > 1) ? rows - 1 : 1;

    auto lerp = [](float a, float b, float t)
    {
        return a + (b - a) * t;
    };

    auto clamp_byte = [](float value)
    {
        if (value < 0.0f)
        {
            value = 0.0f;
        }
        if (value > 255.0f)
        {
            value = 255.0f;
        }
        return static_cast<int>(value + 0.5f);
    };

    std::string output;
    output.reserve(static_cast<size_t>(columns * rows * 20));
    output.append("\x1b[2J\x1b[H");

    for (int y = 0; y < rows; ++y)
    {
        float v = (rows > 1) ? static_cast<float>(y) / static_cast<float>(rows - 1) : 0.0f;
        for (int x = 0; x < columns; ++x)
        {
            float u = (columns > 1) ? static_cast<float>(x) / static_cast<float>(columns - 1) : 0.0f;

            float tl_r = 255.0f;
            float tl_g = 0.0f;
            float tl_b = 0.0f;

            float tr_r = 0.0f;
            float tr_g = 255.0f;
            float tr_b = 0.0f;

            float bl_r = 0.0f;
            float bl_g = 0.0f;
            float bl_b = 255.0f;

            float br_r = 255.0f;
            float br_g = 255.0f;
            float br_b = 255.0f;

            float top_r = lerp(tl_r, tr_r, u);
            float top_g = lerp(tl_g, tr_g, u);
            float top_b = lerp(tl_b, tr_b, u);

            float bottom_r = lerp(bl_r, br_r, u);
            float bottom_g = lerp(bl_g, br_g, u);
            float bottom_b = lerp(bl_b, br_b, u);

            int r = clamp_byte(lerp(top_r, bottom_r, v));
            int g = clamp_byte(lerp(top_g, bottom_g, v));
            int b = clamp_byte(lerp(top_b, bottom_b, v));

            output.append("\x1b[38;2;");
            output.append(std::to_string(r));
            output.push_back(';');
            output.append(std::to_string(g));
            output.push_back(';');
            output.append(std::to_string(b));
            output.append("m█");
        }
        if (y + 1 < rows)
        {
            output.push_back('\n');
        }
    }

    output.append("\x1b[0m");
    output.append("\x1b[1;1H\x1b[31mr\x1b[0m");
    output.append("\x1b[1;");
    output.append(std::to_string(right_col));
    output.append("H\x1b[32mg\x1b[0m");
    output.append("\x1b[");
    output.append(std::to_string(bottom_row));
    output.append(";1H\x1b[34mb\x1b[0m");
    output.append("\x1b[");
    output.append(std::to_string(bottom_row));
    output.append(";");
    output.append(std::to_string(right_col));
    output.append("H0");
    output.append("\x1b[2;2HHello, world!");

    std::cout << output;
    std::cout.flush();
}

Renderer::Renderer(Terminal& terminal)
    : _terminal(terminal)
{
}

Renderer& Renderer::instance()
{
    static Renderer renderer(Terminal::instance());
    return renderer;
}

void Renderer::set_box_colour(const glm::vec3& colour)
{
    _box_colour = colour;
}

void Renderer::set_text_colour(const glm::vec3& colour)
{
    _text_colour = colour;
}

void Renderer::set_canvas(const glm::vec3& colour)
{
    (void)colour;

    glm::ivec2 size = _terminal.get_size();
    if (size.x <= 0 || size.y <= 0)
    {
        return;
    }

    std::vector<Terminal::Character>& background = _terminal.mutate_canvas();
    int width = size.x;
    int height = size.y;
    for (int y = 0; y < height; ++y)
    {
        float t = (height > 1) ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.0f;
        float shade = 0.25f + (0.5f - 0.25f) * t;
        glm::vec3 colour_vec(shade * 255.0f, shade * 255.0f, shade * 255.0f);
        for (int x = 0; x < width; ++x)
        {
            size_t index = static_cast<size_t>(y * width + x);
            if (index >= background.size())
            {
                continue;
            }
            Terminal::Character& cell = background[index];
            cell.set_glyph(U'█');
            cell.set_foreground_colour(colour_vec);
            cell.set_background_colour(colour_vec);
        }
    }

}

void Renderer::draw_box(const glm::ivec2& min_corner, const glm::ivec2& size)
{
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

    std::vector<Terminal::Character>& buffer = _terminal.mutate_buffer();
    int width = terminal_size.x;
    auto set_cell = [&](int x, int y, char32_t glyph)
    {
        if (x < min_x || x > max_x || y < min_y || y > max_y)
        {
            return;
        }

        size_t index = static_cast<size_t>(y * width + x);
        if (index >= buffer.size())
        {
            return;
        }

        buffer[index].set_glyph(glyph);
        buffer[index].set_foreground_colour(_box_colour);
        buffer[index].set_background_colour(_terminal.get_canvas_colour(glm::ivec2(x, y)));
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

    std::vector<Terminal::Character>& buffer = _terminal.mutate_buffer();
    int width = terminal_size.x;
    size_t index = static_cast<size_t>(y * width + start_x);

    for (size_t i = skip; i < text.size() && start_x < terminal_size.x; ++i, ++start_x, ++index)
    {
        if (index >= buffer.size())
        {
            break;
        }

        glm::ivec2 cell_location(start_x, y);
        buffer[index].set_glyph(static_cast<char32_t>(static_cast<unsigned char>(text[i])));
        buffer[index].set_foreground_colour(_text_colour);
        buffer[index].set_background_colour(_terminal.get_canvas_colour(cell_location));
    }
}

void Renderer::draw_string_selected(const std::string& text, const glm::ivec2& location)
{
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

    std::vector<Terminal::Character>& buffer = _terminal.mutate_buffer();
    int width = terminal_size.x;
    size_t index = static_cast<size_t>(y * width + start_x);

    for (size_t i = skip; i < text.size() && start_x < terminal_size.x; ++i, ++start_x, ++index)
    {
        if (index >= buffer.size())
        {
            break;
        }

        glm::ivec2 cell_location(start_x, y);
        Terminal::Character& cell = buffer[index];
        cell.set_glyph(static_cast<char32_t>(static_cast<unsigned char>(text[i])));
        cell.set_foreground_colour(_terminal.get_canvas_colour(cell_location));
        cell.set_background_colour(_text_colour);
    }
}

void draw_browser_row(const app_config& config, const browser_column& column, int row_index, int start_row, bool active)
{
    if (row_index < 0)
    {
        return;
    }

    int row = start_row + row_index;
    std::cout << "\x1b[" << row << ";" << column.start_col << "H";

    auto fg = [&](const app_config::rgb_color& color)
    {
        std::cout << "\x1b[38;2;" << color.r << ";" << color.g << ";" << color.b << "m";
    };
    auto bg = [&](const app_config::rgb_color& color)
    {
        std::cout << "\x1b[48;2;" << color.r << ";" << color.g << ";" << color.b << "m";
    };

    bool is_selected = (column.selected_index == row_index);
    if (is_selected)
    {
        if (active)
        {
            fg(config.browser_selected_fg);
            bg(config.browser_selected_bg);
        }
        else
        {
            fg(config.browser_inactive_fg);
            bg(config.browser_inactive_bg);
        }
    }
    else
    {
        fg(config.browser_normal_fg);
        bg(config.browser_normal_bg);
    }

    std::string line;
    if (row_index < static_cast<int>(column.entries.size()))
    {
        line = column.entries[row_index].name;
    }

    if (static_cast<int>(line.size()) > column.width)
    {
        line.resize(static_cast<size_t>(column.width));
    }
    if (static_cast<int>(line.size()) < column.width)
    {
        line.append(static_cast<size_t>(column.width - line.size()), ' ');
    }

    std::cout << line;
    std::cout << "\x1b[0m";
    std::cout.flush();
}

void draw_browser_column(const app_config& config, const browser_column& column, int start_row, int max_rows, bool active)
{
    if (max_rows <= 0)
    {
        return;
    }

    for (int i = 0; i < max_rows; ++i)
    {
        draw_browser_row(config, column, i, start_row, active);
    }

    std::cout.flush();
}

void draw_browser_box(const app_config& config, const browser_column& column, int start_row, int max_rows)
{
    if (max_rows <= 0)
    {
        return;
    }

    int top_row = start_row - 1;
    int bottom_row = start_row + max_rows;
    int box_start_col = column.start_col - 1;
    int box_width = column.width + 2;

    if (top_row < 1 || box_start_col < 1)
    {
        return;
    }

    std::string horizontal;
    horizontal.reserve(static_cast<size_t>(column.width) * 3);
    for (int i = 0; i < column.width; ++i)
    {
        horizontal.append("─");
    }

    std::string top = std::string("╭") + horizontal + "╮";
    std::string bottom = std::string("╰") + horizontal + "╯";

    std::cout << "\x1b[38;2;" << config.browser_normal_fg.r << ";" << config.browser_normal_fg.g << ";" << config.browser_normal_fg.b << "m";
    std::cout << "\x1b[48;2;" << config.browser_normal_bg.r << ";" << config.browser_normal_bg.g << ";" << config.browser_normal_bg.b << "m";

    std::cout << "\x1b[" << top_row << ";" << box_start_col << "H" << top;
    for (int r = start_row; r < bottom_row; ++r)
    {
        std::cout << "\x1b[" << r << ";" << box_start_col << "H" << "│";
        std::cout << "\x1b[" << r << ";" << (box_start_col + box_width - 1) << "H" << "│";
    }
    std::cout << "\x1b[" << bottom_row << ";" << box_start_col << "H" << bottom;
    std::cout << "\x1b[0m";
    std::cout.flush();
}

void draw_browsers(const app_config& config, browser_column& artist, browser_column& album, browser_column& song, int active_column)
{
    TerminalSize size = get_terminal_size();
    if (size.columns <= 0 || size.rows <= 0)
    {
        return;
    }

    int start_row = 3;
    int max_rows = std::max(0, size.rows - start_row + 1);

    int artist_rows = std::min(max_rows, static_cast<int>(artist.entries.size()));
    int album_rows = std::min(max_rows, static_cast<int>(album.entries.size()));
    int song_rows = std::min(max_rows, static_cast<int>(song.entries.size()));

    draw_browser_box(config, artist, start_row, artist_rows);
    draw_browser_box(config, album, start_row, album_rows);
    draw_browser_box(config, song, start_row, song_rows);

    draw_browser_column(config, artist, start_row, artist_rows, active_column == 0);
    draw_browser_column(config, album, start_row, album_rows, active_column == 1);
    draw_browser_column(config, song, start_row, song_rows, active_column == 2);
}

void draw_status_line(const std::string& text)
{
    std::cout << "\x1b[1;2H" << text << "\x1b[0m";
    std::cout.flush();
}

void draw_metadata_panel(const app_config& config, const track_metadata& meta, int start_col, int start_row)
{
    if (start_col < 1 || start_row < 1)
    {
        return;
    }

    std::vector<std::string> lines;
    if (!meta.title.empty()) lines.push_back("Title: " + meta.title);
    if (!meta.artist.empty()) lines.push_back("Artist: " + meta.artist);
    if (!meta.album.empty()) lines.push_back("Album: " + meta.album);
    if (!meta.date.empty()) lines.push_back("Date: " + meta.date);
    if (!meta.genre.empty()) lines.push_back("Genre: " + meta.genre);
    if (!meta.track.empty()) lines.push_back("Track: " + meta.track);
    if (meta.sample_rate > 0) lines.push_back("Hz: " + std::to_string(meta.sample_rate));
    if (meta.channels > 0) lines.push_back("Channels: " + std::to_string(meta.channels));
    if (meta.duration_ms > 0) lines.push_back("Length: " + std::to_string(meta.duration_ms / 1000) + "s");
    if (meta.bitrate_kbps > 0) lines.push_back("Bitrate: " + std::to_string(meta.bitrate_kbps) + " kbps");
    if (meta.file_size_bytes > 0) lines.push_back("Size: " + std::to_string(meta.file_size_bytes / 1024) + " KB");

    int row = start_row;
    for (auto line : lines)
    {
        if (config.metadata_max_width > 0 && static_cast<int>(line.size()) > config.metadata_max_width)
        {
            line.resize(static_cast<size_t>(config.metadata_max_width));
        }
        std::cout << "\x1b[" << row << ";" << start_col << "H" << line;
        row += 1;
    }
    std::cout.flush();
}

void fill_browser_area(const app_config& config, const browser_column& column, int start_row, int rows)
{
    if (rows <= 0)
    {
        return;
    }

    int box_start_col = column.start_col - 1;
    int box_width = column.width + 2;
    std::string line(static_cast<size_t>(box_width), ' ');
    std::cout << "\x1b[38;2;" << config.browser_normal_bg.r << ";" << config.browser_normal_bg.g << ";" << config.browser_normal_bg.b << "m";
    std::cout << "\x1b[48;2;" << config.browser_normal_bg.r << ";" << config.browser_normal_bg.g << ";" << config.browser_normal_bg.b << "m";

    for (int i = 0; i < rows + 2; ++i)
    {
        int row = start_row - 1 + i;
        std::cout << "\x1b[" << row << ";" << box_start_col << "H" << line;
    }

    std::cout << "\x1b[0m";
}
