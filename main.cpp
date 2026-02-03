#include <iostream>
#include <string>
#include <vector>

#include "player.h"
#include "terminal.h"

#if defined(_WIN32)
#include <windows.h>
#endif

static void draw_frame()
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

int main()
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

    draw_frame();

    const char* path = "01 High For This.mp3";
    return agmp_play_file(path);
}
