#pragma once
#pragma once

#include <string>
#include <vector>

#include "config.h"
#include "terminal.h"

#include <glm/vec4.hpp>


class Renderer
{
public:
    static Renderer* get();
    static Renderer* init(Terminal& terminal);

    explicit Renderer(Terminal& terminal);
    void set_config(const app_config* config);

    void set_canvas(const glm::vec4& colour);
    void set_canvas(const glm::vec4& top_left, const glm::vec4& top_right, const glm::vec4& bottom_left, const glm::vec4& bottom_right);
    void set_canvas(const std::vector<Terminal::Character>& source);

    void draw_box(
        const glm::ivec2& min_corner,
        const glm::ivec2& size,
        const glm::vec4& foreground,
        const glm::vec4& background);
    const Terminal& get_terminal() const;
    void draw_string(const std::string& text, const glm::ivec2& location);
    void draw_string_selected(const std::string& text, const glm::ivec2& location);
    void draw_string_coloured(const std::string& text, const glm::ivec2& location, const glm::vec4& foreground, const glm::vec4& background);
    void draw_string_canvas_bg(const std::string& text, const glm::ivec2& location, const glm::vec4& foreground);
    void draw_glyph(const glm::ivec2& location, char32_t glyph, const glm::vec4& foreground, const glm::vec4& background);
    void clear_box(const glm::ivec2& min_corner, const glm::ivec2& size);
    glm::ivec2 get_terminal_size() const;

private:
    static Renderer* _instance;
    Terminal& _terminal;
    const app_config* _config = nullptr;
    int _next_canvas_listener_id = 1;
};
