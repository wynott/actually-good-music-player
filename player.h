#pragma once
#pragma once

#include <string>

class Browser;
class Queue;
class SpectrumAnalyzer;

struct player_context
{
    std::string title;
    std::string artist;
    std::string album;
    std::string track_path;
    int position_ms = 0;
};

class Player
{
public:
    int play_file(const std::string& path);
    int start_playback(const std::string& path);
    void stop_playback();
    void toggle_pause();
    bool is_done() const;
    bool is_playing() const;
    int get_position_ms() const;
    void seek_ms(int position_ms);
    void set_current_track(const std::string& path);
    const std::string& get_current_track() const;
    void set_context(const player_context& context);
    player_context get_context() const;
    void ensure_context_from_track();
    void set_song_browser(Browser* browser);
    void set_queue(class Queue* queue);
    void set_spectrum_analyzer(SpectrumAnalyzer* analyzer);
    void handle_track_finished();

private:
    void on_track_finished();

    std::string _current_track;
    player_context _context;
    Browser* _song_browser = nullptr;
    SpectrumAnalyzer* _spectrum_analyzer = nullptr;
    Queue* _queue = nullptr;
};
