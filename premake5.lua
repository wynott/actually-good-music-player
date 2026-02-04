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
        "album_art.cpp",
        "album_art.h",
        "art_render.cpp",
        "art_render.h",
        "config.cpp",
        "config.h",
        "http.cpp",
        "http.h",
        "input.cpp",
        "input.h",
        "main.cpp",
        "metadata.cpp",
        "metadata.h",
        "state.cpp",
        "state.h",
        "net.cpp",
        "net.h",
        "terminal.cpp",
        "terminal.h",
        "player.c",
        "player.h",
        "miniaudio.h"
    }

    includedirs { ".", "vendor/glm", "vendor/spdlog/include", "vendor" }

    filter "configurations:Debug"
        symbols "On"
        defines { "DEBUG" }

    filter "configurations:Release"
        optimize "On"
        defines { "NDEBUG" }

    filter "system:linux"
        links { "dl", "pthread", "m", "curl" }

    filter "system:macosx"
        links { "pthread", "m", "curl" }

    filter "system:windows"
        links { "ws2_32", "curl" }

    filter {}
