#include "state.h"

#include <algorithm>
#include <fstream>

#include "browser.h"

static std::string trim(std::string value)
{
    auto is_space = [](unsigned char ch)
    {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    };

    while (!value.empty() && is_space(static_cast<unsigned char>(value.front())))
    {
        value.erase(value.begin());
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back())))
    {
        value.pop_back();
    }
    return value;
}

static std::string strip_quotes(std::string value)
{
    if (value.size() >= 2)
    {
        char first = value.front();
        char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
        {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

app_state load_state(const std::string& path)
{
    app_state state;
    state.context.position_ms = 0;
    state.song_index = 0;

    std::ifstream file(path);
    if (!file)
    {
        return state;
    }

    std::string line;
    while (std::getline(file, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }

        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        value = strip_quotes(value);

        if (key == "last_track")
        {
            state.context.track_path = value;
        }
        else if (key == "last_position_ms")
        {
            try
            {
                state.context.position_ms = std::stoi(value);
            }
            catch (...)
            {
            }
        }
        else if (key == "context_track_path")
        {
            state.context.track_path = value;
        }
        else if (key == "context_position_ms")
        {
            try
            {
                state.context.position_ms = std::stoi(value);
            }
            catch (...)
            {
            }
        }
        else if (key == "context_title")
        {
            state.context.title = value;
        }
        else if (key == "context_artist")
        {
            state.context.artist = value;
        }
        else if (key == "context_album")
        {
            state.context.album = value;
        }
        else if (key == "artist_path")
        {
            state.artist_path = value;
        }
        else if (key == "album_path")
        {
            state.album_path = value;
        }
        else if (key == "song_path")
        {
            state.song_path = value;
        }
        else if (key == "song_index")
        {
            try
            {
                state.song_index = std::stoi(value);
            }
            catch (...)
            {
            }
        }
        else if (key.rfind("queue_item_", 0) == 0)
        {
            std::string index_text = key.substr(11);
            try
            {
                int index = std::stoi(index_text);
                if (index >= 0)
                {
                    if (static_cast<size_t>(index) >= state.queue_paths.size())
                    {
                        state.queue_paths.resize(static_cast<size_t>(index) + 1);
                    }
                    state.queue_paths[static_cast<size_t>(index)] = value;
                }
            }
            catch (...)
            {
            }
        }
    }

    return state;
}

void save_state(const std::string& path, const app_state& state)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file)
    {
        return;
    }

    file << "last_track = \"" << state.context.track_path << "\"\n";
    file << "last_position_ms = " << state.context.position_ms << "\n";
    file << "context_track_path = \"" << state.context.track_path << "\"\n";
    file << "context_position_ms = " << state.context.position_ms << "\n";
    file << "context_title = \"" << state.context.title << "\"\n";
    file << "context_artist = \"" << state.context.artist << "\"\n";
    file << "context_album = \"" << state.context.album << "\"\n";
    file << "artist_path = \"" << state.artist_path << "\"\n";
    file << "album_path = \"" << state.album_path << "\"\n";
    file << "song_path = \"" << state.song_path << "\"\n";
    file << "song_index = " << state.song_index << "\n";
    file << "queue_count = " << state.queue_paths.size() << "\n";
    for (size_t i = 0; i < state.queue_paths.size(); ++i)
    {
        file << "queue_item_" << i << " = \"" << state.queue_paths[i] << "\"\n";
    }
}

void app_state::apply_to_browsers(Browser& artist, Browser& album, Browser& song) const
{
    if (artist_path.empty())
    {
        return;
    }

    artist.set_path(artist_path);
    if (!album_path.empty())
    {
        album.set_path(album_path);
    }
    if (!song_path.empty())
    {
        song.set_path(song_path);
        song.set_selected_index(static_cast<size_t>(std::max(0, song_index)));
    }
}
