#include "queue.h"

#include "queue.h"

#include "app.h"
#include "browser.h"
#include "metadata.h"
#include "draw.h"

Queue::Queue() = default;

Queue::Queue(const glm::ivec2& location, const glm::ivec2& size)
    : _location(location),
      _size(size)
{
}

void Queue::ensure_stop_item()
{
    if (!_items.empty())
    {
        const BrowserItem* tail = _items.back().get();
        if (dynamic_cast<const StopPlayItem*>(tail) != nullptr)
        {
            return;
        }
    }

    _items.push_back(std::make_unique<StopPlayItem>(nullptr, "Stop", std::filesystem::path()));
}

void Queue::set_location(const glm::ivec2& location)
{
    _location = location;
}

void Queue::set_size(const glm::ivec2& size)
{
    _size = size;
}

class QueueEntryItem : public BrowserItem
{
public:
    QueueEntryItem(Browser* owner, const std::string& name, const std::filesystem::path& path)
        : BrowserItem(owner, name, path)
    {
    }

    void on_soft_select() override {}
    void on_select() override {}

    glm::ivec2 get_size() const override
    {
        return glm::ivec2(static_cast<int>(get_name().size()), 1);
    }

    void draw(const glm::ivec2& location, const glm::ivec2& size) const override
    {
        auto renderer = Renderer::get();
        if (!renderer)
        {
            return;
        }

        int width = size.x;
        int height = size.y;
        if (width <= 0 || height <= 0)
        {
            return;
        }

        const app_config& config = ActuallyGoodMP::instance().get_config();
        glm::vec4 normal_fg = config.browser_normal_fg;

        std::string line = get_name();
        if (static_cast<int>(line.size()) > width)
        {
            line.resize(static_cast<size_t>(width));
        }

        if (static_cast<int>(line.size()) < width)
        {
            line.append(static_cast<size_t>(width - line.size()), ' ');
        }

        renderer->draw_string_coloured(line, location, normal_fg, glm::vec4(0.0f));

        if (height > 1)
        {
            renderer->clear_box(
                glm::ivec2(location.x, location.y + 1),
                glm::ivec2(width, height - 1));
        }
    }

    void scan_and_populate(
        const std::filesystem::path&,
        Browser*,
        std::vector<std::unique_ptr<BrowserItem>>& out) const override
    {
        (void)out;
    }
};

void Queue::enqueue(const std::filesystem::path& path)
{
    ensure_stop_item();

    std::string name;
    track_metadata meta;
    if (read_track_metadata(path.string(), meta) && (!meta.artist.empty() || !meta.title.empty()))
    {
        if (!meta.artist.empty() && !meta.title.empty())
        {
            name = meta.artist + " - " + meta.title;
        }
        else if (!meta.title.empty())
        {
            name = meta.title;
        }
        else
        {
            name = meta.artist;
        }
    }

    if (name.empty())
    {
        name = path.filename().string();
        if (name.empty())
        {
            name = path.string();
        }
    }

    if (_items.empty())
    {
        _items.push_back(std::make_unique<QueueEntryItem>(nullptr, name, path));
        ensure_stop_item();
        return;
    }

    size_t insert_index = _items.size();
    if (dynamic_cast<StopPlayItem*>(_items.back().get()) != nullptr)
    {
        insert_index = _items.size() - 1;
    }

    _items.insert(
        _items.begin() + static_cast<std::ptrdiff_t>(insert_index),
        std::make_unique<QueueEntryItem>(nullptr, name, path));
}

bool Queue::pop_next(std::string& out_path)
{
    ensure_stop_item();

    for (size_t i = 0; i < _items.size(); ++i)
    {
        if (_items[i]->get_path().empty())
        {
            continue;
        }

        out_path = _items[i]->get_path().string();
        _items.erase(_items.begin() + static_cast<std::ptrdiff_t>(i));
        return !out_path.empty();
    }

    return false;
}

void Queue::clear()
{
    _items.clear();
}

void Queue::set_paths(const std::vector<std::string>& paths)
{
    _items.clear();
    for (const std::string& path : paths)
    {
        if (!path.empty())
        {
            enqueue(path);
        }
    }
    ensure_stop_item();
}

std::vector<std::string> Queue::get_paths() const
{
    std::vector<std::string> paths;
    paths.reserve(_items.size());
    for (const auto& item : _items)
    {
        std::string path = item->get_path().string();
        if (!path.empty())
        {
            paths.push_back(std::move(path));
        }
    }
    return paths;
}

void Queue::draw(const app_config& config)
{
    auto renderer = Renderer::get();
    if (!renderer)
    {
        return;
    }

    ensure_stop_item();

    int requested_rows = 0;
    for (const auto& item : _items)
    {
        glm::ivec2 item_size = item->get_size();
        requested_rows += std::max(1, item_size.y);
    }

    int desired_height = std::max(3, requested_rows + 3);
    int max_height = std::max(1, config.queue_height);
    int draw_height = std::min(desired_height, max_height);
    _size.y = draw_height;

    glm::ivec2 actual_size = renderer->draw_box(
        _location,
        _size,
        config.ui_box_fg,
        glm::vec4(0.0f));

    int inner_width = actual_size.x - 2;
    int inner_height = actual_size.y - 2;
    if (inner_width <= 0 || inner_height <= 0)
    {
        return;
    }

    std::string header = "Coming up next...";
    if (static_cast<int>(header.size()) > inner_width)
    {
        header.resize(static_cast<size_t>(inner_width));
    }
    renderer->draw_string_coloured(
        header,
        glm::ivec2(_location.x + 1, _location.y + 1),
        config.ui_text_fg,
        glm::vec4(0.0f));

    int list_start_y = _location.y + 2;
    int max_rows = inner_height - 1;
    int cursor_y = list_start_y;
    int max_y = list_start_y + max_rows;
    for (size_t i = 0; i < _items.size(); ++i)
    {
        if (cursor_y >= max_y)
        {
            break;
        }

        const BrowserItem& item = *_items[i];
        glm::ivec2 requested = item.get_size();
        int item_height = std::max(1, requested.y);
        int available_height = std::max(0, max_y - cursor_y);
        if (available_height <= 0)
        {
            break;
        }

        int draw_height = std::min(item_height, available_height);
        glm::ivec2 row_location(_location.x + 1, cursor_y);
        glm::ivec2 row_size(inner_width, draw_height);
        renderer->deselect_region(row_location, row_size);
        item.draw(row_location, row_size);
        cursor_y += item_height;
    }
}
