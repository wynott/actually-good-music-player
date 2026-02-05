#pragma once
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "config.h"

class Player;
class Terminal;
class Renderer;
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
    static void init_all(
        const app_config& config,
        Terminal& terminal,
        Renderer& renderer,
        Browser& artist,
        Browser& album,
        Browser& song,
        Player& player);
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
    void set_focused(bool focused);
    void receive_focus();
    void give_focus(Browser* target);
    void set_left(Browser* left);
    void set_right(Browser* right);
    void set_player(Player* player);
    void set_terminal(Terminal* terminal);
    void set_renderer(Renderer* renderer);
    void set_colours(
        const glm::vec4& normal_fg,
        const glm::vec4& selected_fg,
        const glm::vec4& selected_bg,
        const glm::vec4& inactive_fg,
        const glm::vec4& inactive_bg);

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
    Terminal* _terminal = nullptr;
    Renderer* _renderer = nullptr;
    glm::vec4 _normal_fg = glm::vec4(0.941f, 0.941f, 0.941f, 1.0f);
    glm::vec4 _selected_fg = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec4 _selected_bg = glm::vec4(0.902f, 0.784f, 0.471f, 1.0f);
    glm::vec4 _inactive_fg = glm::vec4(0.078f, 0.078f, 0.078f, 1.0f);
    glm::vec4 _inactive_bg = glm::vec4(0.627f, 0.627f, 0.627f, 1.0f);
    glm::vec4 _canvas_sample = glm::vec4(-1.0f);

private:
    int get_visible_rows() const;
    void update_scroll_for_selection();
};
