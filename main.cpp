#include <iostream>

#include "player.h"

int main()
{
    std::cout << "Hello, world!" << std::endl;

    const char* path = "01 High For This.mp3";
    return agmp_play_file(path);
}
