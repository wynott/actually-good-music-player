#include "app.h"
#include "logging.h"

#include <spdlog/spdlog.h>

int main()
{
    init_logging(spdlog::level::trace);

    auto& app = ActuallyGoodMP::instance();
    app.init();
    app.run();
    app.shutdown();

    shutdown_logging();
}
