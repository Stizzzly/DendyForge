cmake_minimum_required(VERSION 3.20)

project(DendyForge LANGUAGES CXX)

enable_testing()

find_package(SDL3 CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(DendyForgeCpu
    core/cpu/cpu6502.cpp
)

target_include_directories(DendyForgeCpu PUBLIC
    core
)

add_library(DendyForgeCore
    core/ines/ines_reader.cpp
    core/cartridge/cartridge.cpp
    core/cartridge/cartridge_info.cpp
    core/mapper/mapper.cpp
    core/mapper/mapper0.cpp
    core/mapper/mapper2.cpp
    core/bus/bus.cpp
    core/console/console.cpp
    core/controller/controller.cpp
    core/ppu/ppu.cpp
)

target_include_directories(DendyForgeCore PUBLIC
    core
)

target_link_libraries(DendyForgeCore PUBLIC DendyForgeCpu)

add_executable(DendyForgeTests
    tests/test_main.cpp
    tests/cpu/cpu_tests.cpp
    tests/cpu/cpu_modularity_tests.cpp
    tests/bus/bus_tests.cpp
    tests/ppu/ppu_tests.cpp
    tests/console/console_tests.cpp
    tests/controller/controller_tests.cpp
    tests/cartridge/cartridge_tests.cpp
)

target_link_libraries(DendyForgeTests PRIVATE DendyForgeCore)

target_include_directories(DendyForgeTests PRIVATE
    tests
    tests/third_party
)

target_compile_definitions(DendyForgeTests PRIVATE
    DENDYFORGE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_test(NAME DendyForgeTests COMMAND DendyForgeTests)

add_executable(DendyForgeApp
    src/main.cpp
)

target_link_libraries(DendyForgeApp PRIVATE
    DendyForgeCore
    SDL3::SDL3
)

add_custom_command(TARGET DendyForgeApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3>"
        "$<TARGET_FILE_DIR:DendyForgeApp>"
)

if(MINGW)
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

    foreach(runtimeDll
        libgcc_s_seh-1.dll
        libstdc++-6.dll
        libwinpthread-1.dll
    )
        add_custom_command(TARGET DendyForgeTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN_DIR}/${runtimeDll}"
                "$<TARGET_FILE_DIR:DendyForgeTests>"
        )
    endforeach()
endif()
