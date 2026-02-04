#pragma once
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class Player;

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

class Browser;

class BrowserItem
{
public:
    BrowserItem(Browser* owner, const std::string& name, const std::filesystem::path& path);
    virtual ~BrowserItem() = default;

    const std::string& get_name() const;
    const std::filesystem::path& get_path() const;

    virtual void on_soft_select() = 0;
    virtual void on_select() = 0;
    virtual void scan_and_populate(
        const std::filesystem::path& directory,
        Browser* owner,
        std::vector<std::unique_ptr<BrowserItem>>& out) const = 0;

protected:
    BrowserItem() = default;
    Browser* get_owner() const;

private:
    Browser* _owner = nullptr;
    std::string _name;
    std::filesystem::path _path;
};

class FolderItem : public BrowserItem
{
public:
    FolderItem(Browser* owner, const std::string& name, const std::filesystem::path& path);
    FolderItem() = default;

    void on_soft_select() override;
    void on_select() override;
    void scan_and_populate(
        const std::filesystem::path& directory,
        Browser* owner,
        std::vector<std::unique_ptr<BrowserItem>>& out) const override;
};

class Mp3Item : public BrowserItem
{
public:
    Mp3Item(Browser* owner, const std::string& name, const std::filesystem::path& path);
    Mp3Item() = default;

    void on_soft_select() override;
    void on_select() override;
    void scan_and_populate(
        const std::filesystem::path& directory,
        Browser* owner,
        std::vector<std::unique_ptr<BrowserItem>>& out) const override;
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
    void move_selection(int direction);
    void soft_select();
    void resize_to_fit_contents();
    void update(int key);
    void redraw();
    void set_focused(bool focused);
    void receive_focus();
    void give_focus(Browser* target);
    void set_left(Browser* left);
    void set_right(Browser* right);
    void set_player(Player* player);

    const glm::ivec2& get_location() const;
    const glm::ivec2& get_size() const;
    const std::string& get_name() const;
    const std::filesystem::path& get_path() const;
    size_t get_selected_index() const;
    std::filesystem::path get_selected_path() const;
    void highlight_selected();
    bool is_focused() const;
    Browser* get_left() const;
    Browser* get_right() const;
    Player* get_player() const;
    std::string get_next_song_path() const;
    bool advance_to_next_song(std::string& out_path);

    void refresh_contents();
    void refresh();
    void draw() const;

private:
    glm::ivec2 _location = glm::ivec2(0);
    std::string _name;
    glm::ivec2 _size = glm::ivec2(0); // width from config file, height is contents up to available space
    std::vector<std::unique_ptr<BrowserItem>> _contents;
    std::filesystem::path _path;
    size_t _selected_index = 0;
    size_t _scroll_offset = 0;
    bool _is_focused = false;
    Browser* _left = nullptr;
    Browser* _right = nullptr;
    Player* _player = nullptr;

private:
    int get_visible_rows() const;
    void update_scroll_for_selection();
};
