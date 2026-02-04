#pragma once
#pragma once

#include <string>

class Player
{
public:
    int play_file(const std::string& path);
    int start_playback(const std::string& path);
    void stop_playback();
    void toggle_pause();
    bool is_done() const;
    int get_position_ms() const;
    void seek_ms(int position_ms);
    void set_current_track(const std::string& path);
    const std::string& get_current_track() const;

private:
    std::string _current_track;
};
