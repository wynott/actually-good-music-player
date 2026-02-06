#pragma once
#pragma once

#include "actually_good_module.h"
#include "config.h"
#include "terminal.h"

#include <vector>

class Rice : public ActuallyGoodModule
{
public:
    Rice();
    void run(const app_config& config);

    const std::vector<Terminal::Character>& get_buffer() const;
private:
    std::vector<Terminal::Character> _buffer;
};
