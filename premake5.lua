workspace "agmp"
    configurations { "Debug", "Release" }
    startproject "agmp"

project "agmp"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    targetdir "bin/%{cfg.buildcfg}"
    objdir "bin-int/%{cfg.buildcfg}"

    files {
        "main.cpp",
        "terminal.cpp",
        "terminal.h",
        "player.c",
        "player.h",
        "miniaudio.h"
    }

    includedirs { ".", "vendor/glm", "vendor/spdlog/include" }

    filter "configurations:Debug"
        symbols "On"
        defines { "DEBUG" }

    filter "configurations:Release"
        optimize "On"
        defines { "NDEBUG" }

    filter "system:linux"
        links { "dl", "pthread", "m" }

    filter "system:macosx"
        links { "pthread", "m" }

    filter {}
