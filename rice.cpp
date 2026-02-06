#include "rice.h"

#include "app.h"
#include "canvas.h"
#include "draw.h"
#include "spdlog/spdlog.h"

Rice::Rice() = default;

void Rice::run(const app_config& config)
{
    spdlog::trace("Rice::run()");

    auto renderer = Renderer::get();
    if (!renderer)
    {
        return;
    }

    Canvas* canvas = ActuallyGoodMP::instance().get_canvas();
    canvas->resize(renderer->get_terminal_size());
    canvas->build_default(config);
    renderer->set_canvas(canvas->get_buffer());
}
