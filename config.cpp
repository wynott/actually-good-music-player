#include "config.h"

#include <algorithm>
#include <fstream>
#include <string>

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

static app_config::rgb_color parse_color(const std::string& value, app_config::rgb_color fallback)
{
    if (value.size() != 7 || value[0] != '#')
    {
        return fallback;
    }

    auto hex_value = [](char ch) -> int
    {
        if (ch >= '0' && ch <= '9')
        {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f')
        {
            return 10 + (ch - 'a');
        }
        if (ch >= 'A' && ch <= 'F')
        {
            return 10 + (ch - 'A');
        }
        return -1;
    };

    int r1 = hex_value(value[1]);
    int r2 = hex_value(value[2]);
    int g1 = hex_value(value[3]);
    int g2 = hex_value(value[4]);
    int b1 = hex_value(value[5]);
    int b2 = hex_value(value[6]);
    if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0)
    {
        return fallback;
    }

    app_config::rgb_color result;
    result.r = (r1 << 4) | r2;
    result.g = (g1 << 4) | g2;
    result.b = (b1 << 4) | b2;
    return result;
}

app_config load_config(const std::string& path)
{
    app_config config;
    config.default_track = "01 High For This.mp3";
    config.library_path = "library";
    config.play_pause_key = ' ';
    config.quit_key = 'q';
    config.nav_up_key = 'w';
    config.nav_down_key = 's';
    config.nav_left_key = 'a';
    config.nav_right_key = 'd';
    config.use_arrow_keys = true;
    config.enable_online_art = true;
    config.search_key = '/';
    config.auto_resume_playback = true;
    config.safe_mode = false;
    config.browser_normal_fg = {240, 240, 240};
    config.browser_normal_bg = {24, 24, 24};
    config.browser_selected_fg = {0, 0, 0};
    config.browser_selected_bg = {230, 200, 120};
    config.browser_inactive_fg = {20, 20, 20};
    config.browser_inactive_bg = {160, 160, 160};
    config.ui_box_fg = {240, 240, 240};
    config.ui_text_fg = {240, 240, 240};
    config.rice_colour = {247, 185, 85};
    config.rice_background_tl = {22, 22, 27};
    config.rice_background_tr = {21, 22, 27};
    config.rice_background_bl = {23, 24, 29};
    config.rice_background_br = {44, 52, 58};
    config.metadata_max_width = 48;
    config.col_width_artist = 20;
    config.col_width_album = 24;
    config.col_width_song = 30;
    config.art_width_chars = 80;
    config.art_height_chars = 40;
    config.browser_padding = 0;
    config.listen_port = 4242;

    std::ifstream file(path);
    if (!file)
    {
        return config;
    }

    std::string line;
    while (std::getline(file, line))
    {
        line = trim(line);
        if (line.empty())
        {
            continue;
        }
        if (line[0] == '#')
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

        if (key == "default_track")
        {
            config.default_track = value;
        }
        else if (key == "library_path")
        {
            config.library_path = value;
        }
        else if (key == "play_pause_key")
        {
            if (!value.empty())
            {
                config.play_pause_key = value[0];
            }
        }
        else if (key == "quit_key")
        {
            if (!value.empty())
            {
                config.quit_key = value[0];
            }
        }
        else if (key == "nav_up_key")
        {
            if (!value.empty())
            {
                config.nav_up_key = value[0];
            }
        }
        else if (key == "nav_down_key")
        {
            if (!value.empty())
            {
                config.nav_down_key = value[0];
            }
        }
        else if (key == "nav_left_key")
        {
            if (!value.empty())
            {
                config.nav_left_key = value[0];
            }
        }
        else if (key == "nav_right_key")
        {
            if (!value.empty())
            {
                config.nav_right_key = value[0];
            }
        }
        else if (key == "use_arrow_keys")
        {
            if (value == "true" || value == "1" || value == "yes")
            {
                config.use_arrow_keys = true;
            }
            else if (value == "false" || value == "0" || value == "no")
            {
                config.use_arrow_keys = false;
            }
        }
        else if (key == "enable_online_art")
        {
            if (value == "true" || value == "1" || value == "yes")
            {
                config.enable_online_art = true;
            }
            else if (value == "false" || value == "0" || value == "no")
            {
                config.enable_online_art = false;
            }
        }
        else if (key == "search_key")
        {
            if (!value.empty())
            {
                config.search_key = value[0];
            }
        }
        else if (key == "auto_resume_playback")
        {
            if (value == "true" || value == "1" || value == "yes")
            {
                config.auto_resume_playback = true;
            }
            else if (value == "false" || value == "0" || value == "no")
            {
                config.auto_resume_playback = false;
            }
        }
        else if (key == "safe_mode")
        {
            if (value == "true" || value == "1" || value == "yes")
            {
                config.safe_mode = true;
            }
            else if (value == "false" || value == "0" || value == "no")
            {
                config.safe_mode = false;
            }
        }
        else if (key == "browser_normal_fg")
        {
            config.browser_normal_fg = parse_color(value, config.browser_normal_fg);
        }
        else if (key == "browser_normal_bg")
        {
            config.browser_normal_bg = parse_color(value, config.browser_normal_bg);
        }
        else if (key == "browser_selected_fg")
        {
            config.browser_selected_fg = parse_color(value, config.browser_selected_fg);
        }
        else if (key == "browser_selected_bg")
        {
            config.browser_selected_bg = parse_color(value, config.browser_selected_bg);
        }
        else if (key == "browser_inactive_fg")
        {
            config.browser_inactive_fg = parse_color(value, config.browser_inactive_fg);
        }
        else if (key == "browser_inactive_bg")
        {
            config.browser_inactive_bg = parse_color(value, config.browser_inactive_bg);
        }
        else if (key == "ui_box_fg")
        {
            config.ui_box_fg = parse_color(value, config.ui_box_fg);
        }
        else if (key == "ui_text_fg")
        {
            config.ui_text_fg = parse_color(value, config.ui_text_fg);
        }
        else if (key == "rice_colour")
        {
            config.rice_colour = parse_color(value, config.rice_colour);
        }
        else if (key == "rice_background_tl")
        {
            config.rice_background_tl = parse_color(value, config.rice_background_tl);
        }
        else if (key == "rice_background_tr")
        {
            config.rice_background_tr = parse_color(value, config.rice_background_tr);
        }
        else if (key == "rice_background_bl")
        {
            config.rice_background_bl = parse_color(value, config.rice_background_bl);
        }
        else if (key == "rice_background_br")
        {
            config.rice_background_br = parse_color(value, config.rice_background_br);
        }
        else if (key == "metadata_max_width")
        {
            try
            {
                config.metadata_max_width = std::max(10, std::stoi(value));
            }
            catch (...)
            {
            }
        }
        else if (key == "col_width_artist")
        {
            try
            {
                config.col_width_artist = std::max(1, std::stoi(value));
            }
            catch (...)
            {
            }
        }
        else if (key == "col_width_album")
        {
            try
            {
                config.col_width_album = std::max(1, std::stoi(value));
            }
            catch (...)
            {
            }
        }
        else if (key == "col_width_song")
        {
            try
            {
                config.col_width_song = std::max(1, std::stoi(value));
            }
            catch (...)
            {
            }
        }
        else if (key == "art_width_chars")
        {
            try
            {
                config.art_width_chars = std::max(1, std::stoi(value));
            }
            catch (...)
            {
            }
        }
        else if (key == "art_height_chars")
        {
            try
            {
                config.art_height_chars = std::max(1, std::stoi(value));
            }
            catch (...)
            {
            }
        }
        else if (key == "browser_padding")
        {
            try
            {
                config.browser_padding = std::max(0, std::stoi(value));
            }
            catch (...)
            {
            }
        }
        else if (key == "listen_port")
        {
            try
            {
                config.listen_port = std::max(1, std::stoi(value));
            }
            catch (...)
            {
            }
        }
    }

    return config;
}
