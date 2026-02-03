#include "terminal.h"

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
