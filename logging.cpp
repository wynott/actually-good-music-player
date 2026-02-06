#include "logging.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

void init_logging(spdlog::level::level_enum level)
{
    spdlog::drop("agmp");
    auto log = spdlog::basic_logger_mt("agmp", "agmp.log", true);
    spdlog::set_default_logger(log);
    spdlog::set_level(level);
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    spdlog::flush_on(level);
}

void shutdown_logging()
{
    spdlog::shutdown();
}
