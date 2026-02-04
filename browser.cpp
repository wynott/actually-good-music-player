#include "browser.h"

#include <algorithm>

#include "browser.h"
#include "draw.h"

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

void Browser::set_path(const std::filesystem::path& path)
{
    _path = path;
    refresh_contents();
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

void Browser::refresh_contents()
{
    _contents.clear();
    std::vector<browser_column::entry> entries = list_entries(_path);
    _contents.reserve(entries.size());

    for (const browser_column::entry& entry : entries)
    {
        BrowserItem item;
        item.name = entry.name;
        item.path = entry.path;
        item.is_dir = entry.is_dir;
        _contents.push_back(std::move(item));
    }

    if (_contents.empty())
    {
        _selected_index = 0;
    }
    else if (_selected_index >= _contents.size())
    {
        _selected_index = _contents.size() - 1;
    }
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
        const BrowserItem& item = _contents[i];
        std::string line = item.name;
        if (static_cast<int>(line.size()) > inner_width - 2)
        {
            line.resize(static_cast<size_t>(inner_width - 2));
        }

        if (i == _selected_index)
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

        renderer.draw_string(line, glm::ivec2(_location.x + 1, list_start_y + static_cast<int>(i)));
    }
}
