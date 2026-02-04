#pragma once
#pragma once

#include <string>

#include "browser.h"
#include "config.h"
#include "metadata.h"

#include "terminal.h"

void draw_frame();
void draw_browser_row(const app_config& config, const browser_column& column, int row_index, int start_row, bool active);
void draw_browser_column(const app_config& config, const browser_column& column, int start_row, int max_rows, bool active);
void draw_browser_box(const app_config& config, const browser_column& column, int start_row, int max_rows);
void draw_browsers(const app_config& config, browser_column& artist, browser_column& album, browser_column& song, int active_column);
void draw_status_line(const std::string& text);
void draw_metadata_panel(const app_config& config, const track_metadata& meta, int start_col, int start_row);
void fill_browser_area(const app_config& config, const browser_column& column, int start_row, int rows);


class Renderer
{
public:
    explicit Renderer(Terminal& terminal);
    static Renderer& instance();
    void set_canvas(const glm::vec3& colour);
    void set_box_colour(const glm::vec3& colour);
    void set_text_colour(const glm::vec3& colour);

    void draw_box(const glm::ivec2& min_corner, const glm::ivec2& size);
    void draw_string(const std::string& text, const glm::ivec2& location);
    void draw_string_selected(const std::string& text, const glm::ivec2& location);

private:
    Terminal& _terminal;
    glm::vec3 _box_colour = glm::vec3(255.0f);
    glm::vec3 _text_colour = glm::vec3(255.0f);
};
