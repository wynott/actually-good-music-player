#include <iostream>

#include "player.h"

static void print_usage(const char* exe) {
    std::cout << "Usage: " << exe << " <audio-file>" << std::endl;
}

int main(int argc, char** argv) {
    const char* path = (argc >= 2) ? argv[1] : "01 High For This.mp3";
    if (argc < 2) {
        std::cout << "Hello, world!" << std::endl;
    }

    int result = agmp_play_file(path);
    if (result != 0 && argc < 2) {
        print_usage(argv[0]);
    }
    return result;
}
