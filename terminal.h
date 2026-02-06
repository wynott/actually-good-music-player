#pragma once
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/ext/vector_uint3_sized.hpp>

#include "map.h"

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

    class Character8
    {
    public:
        Character8();
        Character8(const std::string& value, const glm::u8vec3& foreground, const glm::u8vec3& background);

        void set_glyph(char32_t glyph);
        char32_t get_glyph() const;

        void set_glyph_colour(const glm::u8vec3& colour);
        void set_background_colour(const glm::u8vec3& colour);
        const glm::u8vec3& get_glyph_colour() const;
        const glm::u8vec3& get_background_colour() const;
        void set_particle_id(uint32_t particle_id);
        uint32_t get_particle_id() const;

        bool operator==(const Character8& other) const;
        bool operator!=(const Character8& other) const;

    private:
        char32_t _glyph = U' ';
        glm::u8vec3 _glyph_colour = glm::u8vec3(0);
        glm::u8vec3 _background_colour = glm::u8vec3(0);
        uint32_t _particle_id = 0;
    };

    class BackingStore
    {
    public:
        BackingStore();
        BackingStore(int width, int height);
        void resize(int width, int height);
        std::vector<Character>& layer(const std::string& name);
        const std::vector<Character>& layer(const std::string& name) const;
        bool has_layer(const std::string& name) const;
        int width = 0;
        int height = 0;
        wynott::map<std::string, std::vector<Character>> layers;
        std::vector<std::string> layer_order;
        std::vector<Character8> pending_frame;
        std::vector<Character8> previous_frame;
        std::vector<bool> dirty;
    };

    Terminal();

    void on_terminal_resize();
    void shutdown();
    void init();
    
    void update();
    void eightbitify();
    void update_eightbit();
    void mark_all_dirty();

    glm::ivec2 get_size() const;
    glm::ivec2 get_location(std::size_t buffer_index) const;
    std::size_t get_index(const glm::ivec2& location) const;
    glm::vec4 get_canvas_sample(const glm::ivec2& location) const;
    bool is_dirty(const glm::ivec2& location) const;

    void set_glyph(char32_t glyph, const glm::ivec2& location);
    void set_glyph(const glm::ivec2& location, char32_t glyph, const glm::vec4& foreground, const glm::vec4& background);
    void set_particle_glyph(const glm::ivec2& location, char32_t glyph, const glm::vec4& foreground, const glm::vec4& background, uint32_t particle_id);
    void clear_cell(const glm::ivec2& location);
    void set_canvas(const std::vector<Character>& source);
    void clear_juice();
    void select_region(const glm::ivec2& location, const glm::ivec2& size);
    void deselect_region(const glm::ivec2& location, const glm::ivec2& size);


private:
    void clear_screen();
    

    glm::ivec2 get_terminal_size() const;
    glm::ivec2& mutate_size();

private:
    glm::ivec2 _size = glm::ivec2(0);
    BackingStore _store;
};
