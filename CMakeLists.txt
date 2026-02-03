cmake_minimum_required(VERSION 3.16)

project(agmp LANGUAGES C CXX)

add_executable(agmp
    main.cpp
    player.c
)

target_include_directories(agmp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(agmp PRIVATE cxx_std_17)

if(APPLE)
    target_link_libraries(agmp PRIVATE pthread m)
elseif(UNIX)
    target_link_libraries(agmp PRIVATE dl pthread m)
endif()
