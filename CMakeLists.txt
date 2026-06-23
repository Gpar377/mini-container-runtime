cmake_minimum_required(VERSION 3.16)
project(mini-container LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# All source files
set(SOURCES
    src/main.cpp
    src/container.cpp
    src/namespaces.cpp
    src/cgroups.cpp
    src/filesystem.cpp
    src/network.cpp
    src/utils.cpp
)

add_executable(mini-container ${SOURCES})

# Linux-only: need pthreads for clone()
target_link_libraries(mini-container PRIVATE pthread)

# Compiler warnings
target_compile_options(mini-container PRIVATE -Wall -Wextra -Wpedantic)
