#pragma once

struct TerminalSize
{
    int columns;
    int rows;
};

TerminalSize get_terminal_size();
