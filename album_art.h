#pragma once
#pragma once

#include "terminal.h"
#include "config.h"

class Player;

#include <atomic>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

struct art_result
{
    bool ready = false;
    bool has_art = false;
    bool online_failed = false;
    std::vector<unsigned char> data;
    std::string error_message;
};

bool load_mp3_embedded_art(const char* path, std::vector<unsigned char>& image_data);


class AlbumArt
{
public:
    AlbumArt();
    AlbumArt(const glm::ivec2& location, const glm::ivec2& size);
    ~AlbumArt();

    void set_location(const glm::ivec2& location);
    void set_size(const glm::ivec2& size);

    const glm::ivec2& get_location() const;
    const glm::ivec2& get_size() const;

    bool load(const std::vector<unsigned char>& image_data);
    void draw() const;

    void average_colour(glm::vec3& top_left, glm::vec3& top_right, glm::vec3& bottom_left, glm::vec3& bottom_right) const;

    void set_track(
        const std::string& path,
        const app_config& config,
        const std::string& artist,
        const std::string& album);
    void set_player(Player* player);
    void update_from_player(
        const app_config& config,
        int origin_x,
        int origin_y);
    bool render_current(
        const app_config& config,
        int origin_x,
        int origin_y,
        app_config::rgb_color* out_avg_color);
    void wait_for_fetch();


private:
    bool load_image_data(const std::vector<unsigned char>& image_data);

private:
    mutable std::mutex _mutex;
    art_result _result;
    std::atomic<bool> _dirty{false};
    std::atomic<bool> _pending{false};
    std::thread _thread;
    std::string _current_track;
    std::string _current_artist;
    std::string _current_album;
    Player* _player = nullptr;

private:
    glm::ivec2 _location = glm::ivec2(0);
    glm::ivec2 _size = glm::ivec2(0);
    std::vector<Terminal::Character> _pixels;
};
