#pragma once

#include <spdlog/spdlog.h>

void init_logging(spdlog::level::level_enum level);
void shutdown_logging();
