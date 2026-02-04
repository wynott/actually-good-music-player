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
        void set_inverted(bool inverted);

        const glm::vec3& get_foreground_colour() const;
        const glm::vec3& get_background_colour() const;
        bool is_inverted() const;

        bool dirty = true;

    private:
        char32_t _glyph = U' ';
        glm::vec3 _foreground_colour = glm::vec3(0);
        glm::vec3 _background_colour = glm::vec3(0);
        bool _is_inverted = false;
    };

    Terminal();
    static Terminal& instance();

    void on_terminal_resize();
    
    void scan_and_write_characters();
    void write_region(const glm::ivec2& min, const glm::ivec2& max);

    glm::ivec2 get_size() const;
    const std::vector<Character>& get_buffer() const;
    std::vector<Character>& mutate_buffer();


private:
    void write_character_to_terminal(const glm::ivec2& location);

    void write_string_to_terminal(const glm::ivec2& location, std::size_t length);    

    glm::ivec2 get_terminal_size() const;
    glm::ivec2& mutate_size();

private:
    glm::ivec2 _size = glm::ivec2(0);
    std::vector<Character> _buffer;
    std::vector<Character> _actual_state;
    glm::vec3 _current_foreground_colour = glm::vec3(0);
    glm::vec3 _current_background_colour = glm::vec3(0);
    bool _has_current_colours = false;
};
