#pragma once

#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include <glm/vec2.hpp>

#include "config.h"

class BrowserItem;

class Queue
{
public:
    Queue();
    Queue(const glm::ivec2& location, const glm::ivec2& size);

    void set_location(const glm::ivec2& location);
    void set_size(const glm::ivec2& size);
    void enqueue(const std::filesystem::path& path);
    void enqueue_front(const std::filesystem::path& path);
    bool pop_next(std::string& out_path);
    void clear();
    void set_paths(const std::vector<std::string>& paths);
    std::vector<std::string> get_paths() const;
    void draw(const app_config& config);

private:
    void ensure_stop_item();
    glm::ivec2 _location = glm::ivec2(0);
    glm::ivec2 _size = glm::ivec2(0);
    std::vector<std::unique_ptr<BrowserItem>> _items;
};
