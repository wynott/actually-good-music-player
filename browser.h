#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "config.h"

struct browser_column
{
    std::filesystem::path path;
    struct entry
    {
        std::string name;
        std::filesystem::path path;
        bool is_dir;
    };

    std::vector<entry> entries;
    int selected_index;
    int width;
    int start_col;
};

std::vector<browser_column::entry> list_entries(const std::filesystem::path& path);
void refresh_column(browser_column& column);
bool update_layout(
    const app_config& config,
    browser_column& artist,
    browser_column& album,
    browser_column& song,
    int box_gap,
    int& art_origin_x,
    int& metadata_origin_x);

///////   NEW BELOW

struct BrowserItem
{
    std::string name;
    std::filesystem::path path;
    bool is_dir = false;
};

class Browser
{
public:
    Browser();
    Browser(const std::string& name, const std::filesystem::path& path, const glm::ivec2& location, const glm::ivec2& size);

    void set_location(const glm::ivec2& location);
    void set_size(const glm::ivec2& size);
    void set_name(const std::string& name);
    void set_path(const std::filesystem::path& path);
    void set_selected_index(size_t index);

    const glm::ivec2& get_location() const;
    const glm::ivec2& get_size() const;
    const std::string& get_name() const;
    const std::filesystem::path& get_path() const;
    size_t get_selected_index() const;

    void refresh_contents();
    void draw() const;

private:
    glm::ivec2 _location = glm::ivec2(0);
    std::string _name;
    glm::ivec2 _size = glm::ivec2(0); // width from config file, height is contents up to available space
    std::vector<BrowserItem> _contents;
    std::filesystem::path _path;
    size_t _selected_index = 0;
};
