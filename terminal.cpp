#include "terminal.h"

#include <iostream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

TerminalSize get_terminal_size()
{
    TerminalSize size{0, 0};

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

    size.columns = static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1);
    size.rows = static_cast<int>(info.srWindow.Bottom - info.srWindow.Top + 1);
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
    {
        size.columns = static_cast<int>(w.ws_col);
        size.rows = static_cast<int>(w.ws_row);
    }
#endif

    return size;
}

void clear_screen_rgb(int r, int g, int b)
{
    TerminalSize size = get_terminal_size();
    if (size.columns <= 0 || size.rows <= 0)
    {
        return;
    }

    if (r < 0)
    {
        r = 0;
    }
    if (r > 255)
    {
        r = 255;
    }
    if (g < 0)
    {
        g = 0;
    }
    if (g > 255)
    {
        g = 255;
    }
    if (b < 0)
    {
        b = 0;
    }
    if (b > 255)
    {
        b = 255;
    }

    std::string line(static_cast<size_t>(size.columns), ' ');
    std::cout << "\x1b[2J\x1b[H";
    std::cout << "\x1b[38;2;" << r << ";" << g << ";" << b << "m";
    std::cout << "\x1b[48;2;" << r << ";" << g << ";" << b << "m";
    for (int y = 0; y < size.rows; ++y)
    {
        std::cout << line;
        if (y + 1 < size.rows)
        {
            std::cout << '\n';
        }
    }
    std::cout << "\x1b[0m";
    std::cout.flush();
}
