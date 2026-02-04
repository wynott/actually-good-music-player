#pragma once

#include <string>

struct app_state
{
    std::string last_track;
    int last_position_ms;
};

app_state load_state(const std::string& path);
void save_state(const std::string& path, const app_state& state);
