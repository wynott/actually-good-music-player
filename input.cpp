#include "input.h"

#if defined(_WIN32)
#include <conio.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace
{
#if !defined(_WIN32)
termios g_original;
bool g_has_original = false;
#endif
}

void input_init()
{
#if !defined(_WIN32)
    termios current;
    if (tcgetattr(STDIN_FILENO, &current) == 0)
    {
        g_original = current;
        g_has_original = true;
        current.c_lflag &= static_cast<unsigned int>(~(ICANON | ECHO));
        tcsetattr(STDIN_FILENO, TCSANOW, &current);

        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (flags != -1)
        {
            fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        }
    }
#endif
}

void input_shutdown()
{
#if !defined(_WIN32)
    if (g_has_original)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_original);
        g_has_original = false;

        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (flags != -1)
        {
            fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
        }
    }
#endif
}

int input_poll_key()
{
#if defined(_WIN32)
    if (_kbhit())
    {
        int ch = _getch();
        if (ch == 0 || ch == 224)
        {
            int code = _getch();
            switch (code)
            {
            case 72:
                return input_key_up;
            case 80:
                return input_key_down;
            case 75:
                return input_key_left;
            case 77:
                return input_key_right;
            default:
                return -1;
            }
        }
        return ch;
    }
    return -1;
#else
    unsigned char ch = 0;
    ssize_t result = read(STDIN_FILENO, &ch, 1);
    if (result == 1)
    {
        if (ch == 0x1b)
        {
            unsigned char seq[2] = {0, 0};
            ssize_t r1 = read(STDIN_FILENO, &seq[0], 1);
            ssize_t r2 = read(STDIN_FILENO, &seq[1], 1);
            if (r1 == 1 && r2 == 1 && seq[0] == '[')
            {
                switch (seq[1])
                {
                case 'A':
                    return input_key_up;
                case 'B':
                    return input_key_down;
                case 'D':
                    return input_key_left;
                case 'C':
                    return input_key_right;
                default:
                    return -1;
                }
            }
            return -1;
        }
        return ch;
    }
    if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
    {
        return -1;
    }
    return -1;
#endif
}
