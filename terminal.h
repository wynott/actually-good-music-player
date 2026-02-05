#pragma once
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

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
        Character(const std::string& value, const glm::vec3& foreground, const glm::vec3& background);

        void set_glyph(char32_t glyph);
        char32_t get_glyph() const;

        void set_foreground_colour(const glm::vec3& colour);
        void set_background_colour(const glm::vec3& colour);
        const glm::vec3& get_foreground_colour() const;
        const glm::vec3& get_background_colour() const;

    private:
        char32_t _glyph = U' ';
        glm::vec3 _foreground_colour = glm::vec3(0);
        glm::vec3 _background_colour = glm::vec3(0);
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
    glm::vec3 get_canvas_colour(const glm::ivec2& location) const;
    bool is_dirty(const glm::ivec2& location) const;

    void set_glyph(char32_t glyph, const glm::ivec2& location);
    void set_glyph(const glm::ivec2& location, char32_t glyph, const glm::vec3& foreground, const glm::vec3& background);
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
    glm::vec3 _current_foreground_colour = glm::vec3(-1.0f);
    glm::vec3 _current_background_colour = glm::vec3(-1.0f);
};
