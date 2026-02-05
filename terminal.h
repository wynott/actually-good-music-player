#pragma once
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

struct TerminalSize
{
    int columns;
    int rows;
};

TerminalSize get_terminal_size();

class Terminal
{
public:
    class Character
    {
    public:
        Character();
        Character(const std::string& value, const glm::vec4& foreground, const glm::vec4& background);

        void set_glyph(char32_t glyph);
        char32_t get_glyph() const;

        void set_glyph_colour(const glm::vec4& colour);
        void set_background_colour(const glm::vec4& colour);
        const glm::vec4& get_glyph_colour() const;
        const glm::vec4& get_background_colour() const;

    private:
        char32_t _glyph = U' ';
        glm::vec4 _glyph_colour = glm::vec4(0.0f);
        glm::vec4 _background_colour = glm::vec4(0.0f);
    };

    class BackingStore
    {
    public:
        BackingStore();
        BackingStore(int width, int height);
        void resize(int width, int height);
        int width = 0;
        int height = 0;
        std::vector<Character> buffer;
        std::vector<Character> canvas;
        std::vector<bool> dirty;
    };

    Terminal();

    void on_terminal_resize();
    void shutdown();
    
    void update();
    void mark_all_dirty();

    glm::ivec2 get_size() const;
    glm::ivec2 get_location(std::size_t buffer_index) const;
    std::size_t get_index(const glm::ivec2& location) const;
    glm::vec4 get_canvas_sample(const glm::ivec2& location) const;
    bool is_dirty(const glm::ivec2& location) const;

    void set_glyph(char32_t glyph, const glm::ivec2& location);
    void set_glyph(const glm::ivec2& location, char32_t glyph, const glm::vec4& foreground, const glm::vec4& background);
    void clear_cell(const glm::ivec2& location);
    void set_canvas(const std::vector<Character>& source);


private:
    void clear_screen();
    void write_string_to_terminal(const glm::ivec2& location, std::size_t length);    

    glm::ivec2 get_terminal_size() const;
    glm::ivec2& mutate_size();

private:
    glm::ivec2 _size = glm::ivec2(0);
    BackingStore _store;
    glm::vec4 _current_glyph_colour = glm::vec4(-1.0f);
    glm::vec4 _current_background_colour = glm::vec4(-1.0f);
};
