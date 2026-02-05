#pragma once
#pragma once

#include <string>

#include "config.h"
#include "terminal.h"


class Renderer
{
public:
    explicit Renderer(Terminal& terminal);
    void set_config(const app_config* config);

    void set_canvas(const glm::vec3& colour);
    void set_canvas(const glm::vec3& top_left, const glm::vec3& top_right, const glm::vec3& bottom_left, const glm::vec3& bottom_right);

    void draw_box(
        const glm::ivec2& min_corner,
        const glm::ivec2& size,
        const glm::vec3& foreground,
        const glm::vec3& background);
    const Terminal& get_terminal() const;
    void draw_string(const std::string& text, const glm::ivec2& location);
    void draw_string_selected(const std::string& text, const glm::ivec2& location);
    void draw_string_coloured(const std::string& text, const glm::ivec2& location, const glm::vec3& foreground, const glm::vec3& background);
    void draw_string_canvas_bg(const std::string& text, const glm::ivec2& location, const glm::vec3& foreground);

private:
    Terminal& _terminal;
    const app_config* _config = nullptr;
    int _next_canvas_listener_id = 1;
};
