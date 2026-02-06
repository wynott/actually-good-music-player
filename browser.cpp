#include "browser.h"

#include <algorithm>
#include <filesystem>

#include <glm/vec4.hpp>

#include "browser.h"
#include "app.h"
#include "draw.h"
#include "event.h"
#include "input.h"
#include "player.h"
#include "spdlog/spdlog.h"

static void draw_item_line(
    const std::string& name,
    const glm::ivec2& location,
    const glm::ivec2& size,
    bool selected,
    bool focused)
{
    auto renderer = Renderer::get();
    if (!renderer)
    {
        return;
    }

    int width = size.x;
    if (width <= 0)
    {
        return;
    }

    const app_config& config = ActuallyGoodMP::instance().get_config();
    glm::vec4 normal_fg = config.browser_normal_fg;
    glm::vec4 selected_fg = config.browser_selected_fg;
    glm::vec4 selected_bg = config.browser_selected_bg;
    glm::vec4 unfocused_selected_fg = config.browser_inactive_bg;

    std::string line = name;
    int max_name = std::max(0, width - 1);
    if (static_cast<int>(line.size()) > max_name)
    {
        line.resize(static_cast<size_t>(max_name));
    }

    if (selected)
    {
        line.insert(line.begin(), '>');
    }
    else
    {
        line.insert(line.begin(), ' ');
    }

    if (static_cast<int>(line.size()) < width)
    {
        line.append(static_cast<size_t>(width - line.size()), ' ');
    }

    if (selected && focused)
    {
        renderer->draw_string_coloured(line, location, selected_fg, selected_bg);
    }
    else if (selected)
    {
        renderer->draw_string_canvas_bg(line, location, unfocused_selected_fg);
    }
    else
    {
        renderer->draw_string_canvas_bg(line, location, normal_fg);
    }
}

Browser::Browser() = default;

Browser::Browser(
    const std::string& name,
    const std::filesystem::path& path,
    const glm::ivec2& location,
    const glm::ivec2& size)
    : _location(location),
      _name(name),
      _size(size),
      _path(path)
{
    refresh_contents();
}



void Browser::set_location(const glm::ivec2& location)
{
    _location = location;
}

void Browser::set_size(const glm::ivec2& size)
{
    _size = size;
}

void Browser::set_name(const std::string& name)
{
    _name = name;
}

BrowserItem::BrowserItem(Browser* owner, const std::string& name, const std::filesystem::path& path)
    : _owner(owner),
      _name(name),
      _path(path)
{
}

const std::string& BrowserItem::get_name() const
{
    return _name;
}

const std::filesystem::path& BrowserItem::get_path() const
{
    return _path;
}

Browser* BrowserItem::get_owner() const
{
    return _owner;
}

FolderItem::FolderItem(Browser* owner, const std::string& name, const std::filesystem::path& path)
    : BrowserItem(owner, name, path)
{
}

void FolderItem::on_soft_select()
{
    Browser* owner = get_owner();
    if (owner == nullptr)
    {
        return;
    }

    Browser* right = owner->get_right();
    if (right == nullptr)
    {
        return;
    }

    right->set_path(get_path());
    right->soft_select();
    right->draw();
}

glm::ivec2 FolderItem::get_size() const
{
    return glm::ivec2(static_cast<int>(get_name().size()) + 1, 1);
}

void FolderItem::draw(
    const glm::ivec2& location,
    const glm::ivec2& size,
    bool selected,
    bool focused) const
{
    draw_item_line(get_name(), location, size, selected, focused);
}

void FolderItem::scan_and_populate(
    const std::filesystem::path& directory,
    Browser* owner,
    std::vector<std::unique_ptr<BrowserItem>>& out) const
{
    namespace fs = std::filesystem;
    try
    {
        if (!fs::exists(directory) || !fs::is_directory(directory))
        {
            return;
        }

        for (const auto& entry : fs::directory_iterator(directory))
        {
            if (!entry.is_directory())
            {
                continue;
            }
            std::string name = entry.path().filename().string();
            out.push_back(std::make_unique<FolderItem>(owner, name, entry.path()));
        }
    }
    catch (...)
    {
    }
}

void FolderItem::on_select()
{
}


Mp3Item::Mp3Item(Browser* owner, const std::string& name, const std::filesystem::path& path)
    : BrowserItem(owner, name, path)
{
}

void Mp3Item::on_soft_select()
{
}

void Mp3Item::on_select()
{
    Browser* owner = get_owner();
    if (!owner)
    {
        return;
    }

    EventBus::instance().publish(Event{
        "browser.mp3_selected",
        get_path().string()
    });
}

glm::ivec2 Mp3Item::get_size() const
{
    return glm::ivec2(static_cast<int>(get_name().size()) + 1, 1);
}

void Mp3Item::draw(
    const glm::ivec2& location,
    const glm::ivec2& size,
    bool selected,
    bool focused) const
{
    draw_item_line(get_name(), location, size, selected, focused);
}

void Mp3Item::scan_and_populate(
    const std::filesystem::path& directory,
    Browser* owner,
    std::vector<std::unique_ptr<BrowserItem>>& out) const
{
    namespace fs = std::filesystem;
    try
    {
        if (!fs::exists(directory) || !fs::is_directory(directory))
        {
            return;
        }

        for (const auto& entry : fs::directory_iterator(directory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            fs::path path = entry.path();
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            if (extension != ".mp3")
            {
                continue;
            }
            std::string name = path.filename().string();
            out.push_back(std::make_unique<Mp3Item>(owner, name, path));
        }
    }
    catch (...)
    {
    }
}


void Browser::set_path(const std::filesystem::path& path)
{
    _path = path;
    refresh_contents();
    set_selected_index(0);
    soft_select();
}

void Browser::set_selected_index(size_t index)
{
    _selected_index = index;
    if (_contents.empty())
    {
        _selected_index = 0;
        _scroll_offset = 0;
        return;
    }

    if (_selected_index >= _contents.size())
    {
        _selected_index = _contents.size() - 1;
    }

    update_scroll_for_selection();
}

void Browser::move_selection(int direction)
{
    if (!_is_focused)
    {
        return;
    }

    if (_contents.empty())
    {
        _selected_index = 0;
        return;
    }

    int next = static_cast<int>(_selected_index) + direction;
    next = std::clamp(next, 0, static_cast<int>(_contents.size() - 1));
    set_selected_index(static_cast<size_t>(next));

    draw();

    if (_right)
    {
        soft_select();
    }
}

void Browser::resize_to_fit_contents()
{
    spdlog::trace("Browser::resize_to_fit_contents() begin");

    glm::ivec2 previous_size = _size;
    int rows = 0;
    int max_width = 1;
    for (const auto& item : _contents)
    {
        glm::ivec2 item_size = item->get_size();
        rows += std::max(1, item_size.y);
        max_width = std::max(max_width, item_size.x);
    }

    int required_height = rows + 2;
    if (required_height < 3)
    {
        required_height = 3;
    }
    _size.y = required_height;

    int required_width = std::max(3, max_width + 2);
    _size.x = required_width;

    auto renderer = Renderer::get();
    if (!renderer)
    {
        return;
    }
    auto terminal_size = renderer->get_terminal_size();

    if (terminal_size.y > 0)
    {
        int max_height = terminal_size.y - 1 - _location.y;
        if (max_height > 0 && _size.y > max_height)
        {
            _size.y = max_height;
        }
    }

    if (terminal_size.x > 0)
    {
        int max_width_available = terminal_size.x - 1 - _location.x;
        if (max_width_available > 0 && _size.x > max_width_available)
        {
            _size.x = max_width_available;
        }
    }

    glm::ivec2 min_corner = _location;
    glm::ivec2 max_size(
        std::max(previous_size.x, _size.x),
        std::max(previous_size.y, _size.y));

    if (max_size.x <= 0 || max_size.y <= 0)
    {
        return;
    }

    if (terminal_size.x <= 0 || terminal_size.y <= 0)
    {
        return;
    }

    int start_x = std::max(0, min_corner.x);
    int start_y = std::max(0, min_corner.y);

    int max_x = std::min(terminal_size.x, min_corner.x + max_size.x);
    int max_y = std::min(terminal_size.y, min_corner.y + max_size.y);

    if (start_x >= max_x || start_y >= max_y)
    {
        return;
    }

    int new_max_x = std::min(terminal_size.x, min_corner.x + _size.x);
    int new_max_y = std::min(terminal_size.y, min_corner.y + _size.y);

    if (new_max_x < max_x)
    {
        renderer->clear_box(
            glm::ivec2(new_max_x, start_y),
            glm::ivec2(max_x - new_max_x, max_y - start_y));
    }

    if (new_max_y < max_y)
    {
        renderer->clear_box(
            glm::ivec2(start_x, new_max_y),
            glm::ivec2(new_max_x - start_x, max_y - new_max_y));
    }

    spdlog::trace("Browser::resize_to_fit_contents() begin");
}

void Browser::update(int key)
{
    Browser* head = this;
    while (head->_left)
    {
        head = head->_left;
    }

    Browser* focused = head;
    for (Browser* current = head; current; current = current->_right)
    {
        if (current->_is_focused)
        {
            focused = current;
            break;
        }
    }

    if (key == input_key_left)
    {
        Browser* left = focused->get_left();
        if (left)
        {
            focused->give_focus(left);
        }
        return;
    }

    if (key == input_key_right)
    {
        Browser* right = focused->get_right();
        if (right)
        {
            focused->give_focus(right);
        }
        return;
    }

    if (key == input_key_up || key == input_key_down)
    {
        int direction = (key == input_key_up) ? -1 : 1;
        focused->move_selection(direction);
        return;
    }

    if (key == '\r' || key == '\n')
    {
        if (!focused->_contents.empty() && focused->_selected_index < focused->_contents.size())
        {
            focused->_contents[focused->_selected_index]->on_select();
        }
    }
}

void Browser::set_focused(bool focused)
{
    _is_focused = focused;
}

void Browser::receive_focus()
{
    _is_focused = true;
}

void Browser::give_focus(Browser* target)
{
    if (!target)
    {
        return;
    }

    _is_focused = false;
    target->receive_focus();
}

void Browser::set_left(Browser* left)
{
    _left = left;
}

void Browser::set_right(Browser* right)
{
    _right = right;
}

const glm::ivec2& Browser::get_location() const
{
    return _location;
}

const glm::ivec2& Browser::get_size() const
{
    return _size;
}

const std::string& Browser::get_name() const
{
    return _name;
}

const std::filesystem::path& Browser::get_path() const
{
    return _path;
}

size_t Browser::get_selected_index() const
{
    return _selected_index;
}

std::filesystem::path Browser::get_selected_path() const
{
    if (_contents.empty() || _selected_index >= _contents.size())
    {
        return std::filesystem::path();
    }

    return _contents[_selected_index]->get_path();
}

bool Browser::is_focused() const
{
    return _is_focused;
}

Browser* Browser::get_left() const
{
    return _left;
}

Browser* Browser::get_right() const
{
    return _right;
}

std::string Browser::get_next_song_path() const
{
    if (_contents.empty())
    {
        return std::string();
    }

    size_t index = _selected_index + 1;
    while (index < _contents.size())
    {
        if (dynamic_cast<const Mp3Item*>(_contents[index].get()) != nullptr)
        {
            return _contents[index]->get_path().string();
        }
        index += 1;
    }

    return std::string();
}

bool Browser::advance_to_next_song(std::string& out_path)
{
    if (_contents.empty())
    {
        return false;
    }

    size_t index = _selected_index + 1;
    while (index < _contents.size())
    {
        if (dynamic_cast<const Mp3Item*>(_contents[index].get()) != nullptr)
        {
            set_selected_index(index);
            out_path = _contents[index]->get_path().string();
            return true;
        }
        index += 1;
    }

    return false;
}

void Browser::refresh_contents()
{
    _contents.clear();
    _scroll_offset = 0;
    FolderItem folder_scanner(this, std::string(), std::filesystem::path());
    Mp3Item mp3_scanner(this, std::string(), std::filesystem::path());
    folder_scanner.scan_and_populate(_path, this, _contents);
    mp3_scanner.scan_and_populate(_path, this, _contents);

    std::sort(_contents.begin(), _contents.end(), [](const std::unique_ptr<BrowserItem>& a, const std::unique_ptr<BrowserItem>& b)
    {
        return a->get_name() < b->get_name();
    });

    if (_contents.empty())
    {
        _selected_index = 0;
    }
    else if (_selected_index >= _contents.size())
    {
        _selected_index = _contents.size() - 1;
    }

    resize_to_fit_contents();
}

void Browser::refresh()
{
    refresh_contents();
    set_selected_index(0);
    soft_select();
}

void Browser::draw() const
{
    if (_size.x <= 1 || _size.y <= 1)
    {
        return;
    }

    auto renderer = Renderer::get();
    if (!renderer)
    {
        return;
    }

    renderer->draw_box(
        _location,
        _size,
        ActuallyGoodMP::instance().get_config().browser_normal_fg,
        glm::vec4(0.0f));

    int inner_width = _size.x - 2;
    int inner_height = _size.y - 2;
    if (inner_width <= 0 || inner_height <= 0)
    {
        return;
    }

    int list_start_y = _location.y + 1;
    int available_rows = inner_height;
    if (available_rows <= 0)
    {
        return;
    }

    bool has_overflow = _contents.size() > static_cast<size_t>(available_rows);
    int bottom_inner_y = _location.y + _size.y - 2;
    int max_entry_y = has_overflow ? bottom_inner_y - 1 : bottom_inner_y;
    int max_entry_rows = max_entry_y - list_start_y + 1;
    if (max_entry_rows <= 0)
    {
        return;
    }

    size_t start_index = _scroll_offset;
    size_t row_count = std::min(static_cast<size_t>(max_entry_rows), _contents.size() - start_index);
    for (size_t i = 0; i < row_count; ++i)
    {
        size_t item_index = start_index + i;
        const BrowserItem& item = *_contents[item_index];
        glm::ivec2 row_location(_location.x + 1, list_start_y + static_cast<int>(i));
        glm::ivec2 row_size(inner_width, 1);
        bool is_selected = (item_index == _selected_index);
        item.draw(row_location, row_size, is_selected, _is_focused);
    }

    if (has_overflow && available_rows > 0 && inner_width > 0)
    {
        std::string indicator(static_cast<size_t>(inner_width), ' ');
        size_t arrow_index = static_cast<size_t>(std::max(0, inner_width / 2));
        if (arrow_index < indicator.size())
        {
            indicator[arrow_index] = 'v';
        }
        renderer->draw_string(indicator, glm::ivec2(_location.x + 1, bottom_inner_y));
    }
}

int Browser::get_visible_rows() const
{
    int inner_height = _size.y - 2;
    int available_rows = inner_height;
    if (available_rows <= 0)
    {
        return 0;
    }

    bool has_overflow = _contents.size() > static_cast<size_t>(available_rows);
    int visible_rows = available_rows;
    if (has_overflow && visible_rows > 0)
    {
        visible_rows -= 1;
    }
    return std::max(0, visible_rows);
}

void Browser::update_scroll_for_selection()
{
    int visible_rows = get_visible_rows();
    if (visible_rows <= 0)
    {
        _scroll_offset = 0;
        return;
    }

    if (_selected_index < _scroll_offset)
    {
        _scroll_offset = _selected_index;
        return;
    }

    size_t end_index = _scroll_offset + static_cast<size_t>(visible_rows);
    if (_selected_index >= end_index)
    {
        _scroll_offset = _selected_index - static_cast<size_t>(visible_rows - 1);
    }
}
void Browser::soft_select()
{
    if (_contents.empty() || _selected_index >= _contents.size())
    {
        return;
    }

    if (_right != nullptr)
    {
        if (dynamic_cast<const FolderItem*>(_contents[_selected_index].get()) == nullptr)
        {
            size_t folder_index = _contents.size();
            for (size_t i = 0; i < _contents.size(); ++i)
            {
                if (dynamic_cast<const FolderItem*>(_contents[i].get()) != nullptr)
                {
                    folder_index = i;
                    break;
                }
            }

            if (folder_index < _contents.size())
            {
                set_selected_index(folder_index);
            }
            else
            {
                _right->set_path(std::filesystem::path());
                return;
            }
        }
    }

    _contents[_selected_index]->on_soft_select();
}
