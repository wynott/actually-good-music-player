#pragma once

#include <glm/vec2.hpp>

class ActuallyGoodModule
{
public:
    virtual ~ActuallyGoodModule() = default;

    void set_location(const glm::ivec2& location)
    {
        _location = location;
    }

    void set_size(const glm::ivec2& size)
    {
        _size = size;
    }

    const glm::ivec2& get_location() const
    {
        return _location;
    }

    const glm::ivec2& get_size() const
    {
        return _size;
    }

protected:
    glm::ivec2 _location = glm::ivec2(0);
    glm::ivec2 _size = glm::ivec2(0);
};
