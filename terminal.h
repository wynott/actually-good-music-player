#pragma once

struct TerminalSize
{
    int columns;
    int rows;
};

TerminalSize get_terminal_size();
void clear_screen_rgb(int r, int g, int b);
