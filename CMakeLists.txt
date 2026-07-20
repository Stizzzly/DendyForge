cmake_minimum_required(VERSION 3.20)

project(DendyForge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(DendyForge
    src/main.cpp
    core/ines/ines_reader.cpp
    core/ines/ines.cpp
)

target_include_directories(DendyForge PRIVATE
    core
)
