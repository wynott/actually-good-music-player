#include "browser.h"

#include <algorithm>

#include "browser.h"
#include "draw.h"
#include "input.h"
#include "terminal.h"

std::vector<browser_column::entry> list_entries(const std::filesystem::path& path)
{
    namespace fs = std::filesystem;
    std::vector<browser_column::entry> result;
    try
    {
        if (fs::exists(path) && fs::is_directory(path))
        {
            for (const auto& entry : fs::directory_iterator(path))
            {
                browser_column::entry item;
                item.name = entry.path().filename().string();
                item.path = entry.path();
                item.is_dir = entry.is_directory();
                result.push_back(item);
            }
        }
    }
    catch (...)
    {
    }

    std::sort(result.begin(), result.end(), [](const browser_column::entry& a, const browser_column::entry& b)
    {
        return a.name < b.name;
    });
    return result;
}

void refresh_column(browser_column& column)
{
    column.entries = list_entries(column.path);
    if (column.entries.empty())
    {
        column.selected_index = -1;
    }
    else
    {
        column.selected_index = std::clamp(column.selected_index, 0, static_cast<int>(column.entries.size() - 1));
    }
}

bool update_layout(
    const app_config& config,
    browser_column& artist,
    browser_column& album,
    browser_column& song,
    int box_gap,
    int& art_origin_x,
    int& metadata_origin_x)
{
    int prev_art_origin = art_origin_x;

    artist.width = std::max(1, config.col_width_artist);
    album.width = std::max(1, config.col_width_album);
    song.width = std::max(1, config.col_width_song);

    int artist_box_start = 1;
    int artist_box_width = artist.width + 2;
    int album_box_start = artist_box_start + artist_box_width + box_gap;
    int album_box_width = album.width + 2;
    int song_box_start = album_box_start + album_box_width + box_gap;
    int song_box_width = song.width + 2;

    artist.start_col = artist_box_start + 1;
    album.start_col = album_box_start + 1;
    song.start_col = song_box_start + 1;

    int browser_width = artist_box_width + album_box_width + song_box_width + box_gap * 2;
    art_origin_x = std::max(0, browser_width + box_gap);
    metadata_origin_x = art_origin_x + config.art_width_chars + box_gap;

    return prev_art_origin != art_origin_x;
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
    {
        glm::ivec2 min_corner = right->get_location();
        glm::ivec2 max_corner = min_corner + right->get_size() - glm::ivec2(1);
        Terminal::instance().write_region(min_corner, max_corner);
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
        return;
    }

    if (_selected_index >= _contents.size())
    {
        _selected_index = _contents.size() - 1;
    }
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
    {
        glm::ivec2 min_corner = get_location();
        glm::ivec2 max_corner = min_corner + get_size() - glm::ivec2(1);
        Terminal::instance().write_region(min_corner, max_corner);
    }

    if (_right)
    {
        soft_select();
    }
}

void Browser::resize_to_fit_contents()
{
    glm::ivec2 previous_size = _size;
    size_t rows = _contents.size();
    int required = static_cast<int>(rows + 3);
    if (required < 3)
    {
        required = 3;
    }
    _size.y = required;

    glm::ivec2 min_corner = _location;
    glm::ivec2 max_size(
        std::max(previous_size.x, _size.x),
        std::max(previous_size.y, _size.y));
    if (max_size.x > 0 && max_size.y > 0)
    {
        Terminal& terminal = Terminal::instance();
        glm::ivec2 terminal_size = terminal.get_size();
        if (terminal_size.x > 0 && terminal_size.y > 0)
        {
            int start_x = std::max(0, min_corner.x);
            int start_y = std::max(0, min_corner.y);
            int max_x = std::min(terminal_size.x, min_corner.x + max_size.x);
            int max_y = std::min(terminal_size.y, min_corner.y + max_size.y);
            int new_max_x = std::min(terminal_size.x, min_corner.x + _size.x);
            int new_max_y = std::min(terminal_size.y, min_corner.y + _size.y);

            if (start_x < max_x && start_y < max_y)
            {
                std::vector<Terminal::Character>& buffer = terminal.mutate_buffer();
                int width = terminal_size.x;
                for (int y = start_y; y < max_y; ++y)
                {
                    for (int x = start_x; x < max_x; ++x)
                    {
                        if (x >= new_max_x || y >= new_max_y)
                        {
                            size_t index = static_cast<size_t>(y * width + x);
                            if (index < buffer.size())
                            {
                                buffer[index].set_glyph(U' ');
                            }
                        }
                    }
                }
            }
        }

        glm::ivec2 max_corner = min_corner + max_size - glm::ivec2(1);
        Terminal::instance().write_region(min_corner, max_corner);
    }
}

void Browser::redraw()
{
    draw();
    glm::ivec2 min_corner = get_location();
    glm::ivec2 max_corner = min_corner + get_size() - glm::ivec2(1);
    Terminal::instance().write_region(min_corner, max_corner);
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
            focused->redraw();
            left->redraw();
        }
        return;
    }

    if (key == input_key_right)
    {
        Browser* right = focused->get_right();
        if (right)
        {
            focused->give_focus(right);
            focused->redraw();
            right->redraw();
        }
        return;
    }

    if (key == input_key_up || key == input_key_down)
    {
        int direction = (key == input_key_up) ? -1 : 1;
        focused->move_selection(direction);
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

void Browser::refresh_contents()
{
    _contents.clear();
    std::vector<browser_column::entry> entries = list_entries(_path);
    _contents.reserve(entries.size());

    for (const browser_column::entry& entry : entries)
    {
        if (entry.is_dir)
        {
            _contents.push_back(std::make_unique<FolderItem>(this, entry.name, entry.path));
        }
        else
        {
            _contents.push_back(std::make_unique<Mp3Item>(this, entry.name, entry.path));
        }
    }

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

    Renderer& renderer = Renderer::instance();
    renderer.draw_box(_location, _size);

    int inner_width = _size.x - 2;
    int inner_height = _size.y - 2;
    if (inner_width <= 0 || inner_height <= 0)
    {
        return;
    }

    glm::ivec2 name_pos(_location.x + 1, _location.y + 1);
    std::string title = _name.empty() ? std::string("Browser") : _name;
    if (static_cast<int>(title.size()) > inner_width)
    {
        title.resize(static_cast<size_t>(inner_width));
    }
    renderer.draw_string(title, name_pos);

    int list_start_y = name_pos.y + 1;
    int available_rows = inner_height - 1;
    if (available_rows <= 0)
    {
        return;
    }

    size_t max_rows = static_cast<size_t>(available_rows);
    size_t row_count = std::min(_contents.size(), max_rows);
    for (size_t i = 0; i < row_count; ++i)
    {
        const BrowserItem& item = *_contents[i];
        std::string line = item.get_name();
        if (static_cast<int>(line.size()) > inner_width - 2)
        {
            line.resize(static_cast<size_t>(inner_width - 2));
        }

        bool is_selected = (i == _selected_index);
        if (is_selected)
        {
            line.insert(line.begin(), '>');
        }
        else
        {
            line.insert(line.begin(), ' ');
        }

        if (static_cast<int>(line.size()) < inner_width)
        {
            line.append(static_cast<size_t>(inner_width - line.size()), ' ');
        }

        glm::ivec2 row_location(_location.x + 1, list_start_y + static_cast<int>(i));
        if (is_selected && _is_focused)
        {
            renderer.draw_string_selected(line, row_location);
        }
        else
        {
            renderer.draw_string(line, row_location);
        }
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
