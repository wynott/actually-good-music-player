#include "state.h"

#include <fstream>

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
    state.last_position_ms = 0;

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
            state.last_track = value;
        }
        else if (key == "last_position_ms")
        {
            try
            {
                state.last_position_ms = std::stoi(value);
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

    file << "last_track = \"" << state.last_track << "\"\n";
    file << "last_position_ms = " << state.last_position_ms << "\n";
}
