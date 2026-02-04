#pragma once
#pragma once

#include "terminal.h"


#include <string>
#include <vector>

bool load_mp3_embedded_art(const char* path, std::vector<unsigned char>& image_data);


class AlbumArt
{
public:
    AlbumArt();
    AlbumArt(const glm::ivec2& location, const glm::ivec2& size);

    void set_location(const glm::ivec2& location);
    void set_size(const glm::ivec2& size);

    const glm::ivec2& get_location() const;
    const glm::ivec2& get_size() const;

    bool load(const std::vector<unsigned char>& image_data);
    void draw() const;

private:
    bool load_image_data(const std::vector<unsigned char>& image_data);

private:
    glm::ivec2 _location = glm::ivec2(0);
    glm::ivec2 _size = glm::ivec2(0);
    std::vector<Terminal::Character> _pixels;
};
