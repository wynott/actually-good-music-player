#pragma once
#pragma once

#include "terminal.h"
#include "config.h"

class Player;
class Renderer;

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

    bool get_quadrant_colours(glm::vec4& top_left, glm::vec4& top_right, glm::vec4& bottom_left, glm::vec4& bottom_right) const;

    void set_track(
        const std::string& path,
        const app_config& config,
        const std::string& artist,
        const std::string& album);

    bool refresh(
        const app_config& config,
        int origin_x,
        int origin_y);
    void wait_for_fetch();


private:
    void average_colour(glm::vec4& top_left, glm::vec4& top_right, glm::vec4& bottom_left, glm::vec4& bottom_right) const;
    bool render_current(
        const app_config& config,
        int origin_x,
        int origin_y);
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

private:
    glm::ivec2 _location = glm::ivec2(0);
    glm::ivec2 _size = glm::ivec2(0);
    std::vector<Terminal::Character> _pixels;
};
