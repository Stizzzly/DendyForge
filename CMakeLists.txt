cmake_minimum_required(VERSION 3.20)

project(DendyForge LANGUAGES CXX)

enable_testing()

if(MINGW)
    get_filename_component(DENDYFORGE_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(DENDYFORGE_MINGW_ROOT "${DENDYFORGE_MINGW_BIN_DIR}" DIRECTORY)
    list(PREPEND CMAKE_PREFIX_PATH "${DENDYFORGE_MINGW_ROOT}")
endif()

find_package(SDL3 CONFIG REQUIRED)
find_package(OpenSSL CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(third_party/forge6502 EXCLUDE_FROM_ALL)

add_library(DendyForgeCpu INTERFACE)

target_include_directories(DendyForgeCpu INTERFACE
    core
)

target_link_libraries(DendyForgeCpu INTERFACE Forge6502::Core)

add_library(DendyForgeCore
    core/apu/apu.cpp
    core/ines/ines_reader.cpp
    core/cartridge/cartridge.cpp
    core/cartridge/cartridge_info.cpp
    core/mapper/mapper.cpp
    core/mapper/mapper0.cpp
    core/mapper/mapper1.cpp
    core/mapper/mapper2.cpp
    core/mapper/mapper3.cpp
    core/mapper/mapper4.cpp
    core/mapper/mapper7.cpp
    core/bus/bus.cpp
    core/console/console.cpp
    core/controller/controller.cpp
    core/zapper/zapper.cpp
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
    tests/cpu/cpu_bus_cycle_tests.cpp
    tests/cpu/klaus_functional_tests.cpp
    tests/cpu/blargg_cpu_tests.cpp
    tests/cpu/blargg_timing_tests.cpp
    tests/cpu/blargg_reset_tests.cpp
    tests/cpu/nestest_trace_tests.cpp
    tests/bus/bus_tests.cpp
    tests/ppu/ppu_tests.cpp
    tests/ppu/ppu_timing_tests.cpp
    tests/console/console_tests.cpp
    tests/apu/apu_tests.cpp
    tests/controller/controller_tests.cpp
    tests/cartridge/cartridge_tests.cpp
    tests/mapper/mapper1_tests.cpp
    tests/zapper/zapper_tests.cpp
    tests/mapper/mapper3_tests.cpp
    tests/mapper/mapper4_tests.cpp
    tests/mapper/mapper7_tests.cpp
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
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_sdl3.cpp
    third_party/imgui/backends/imgui_impl_sdlrenderer3.cpp
)

target_include_directories(DendyForgeApp PRIVATE
    third_party/imgui
    third_party/imgui/backends
    third_party/httplib
    third_party
    third_party/stb
)

target_compile_definitions(DendyForgeApp PRIVATE
    DENDYFORGE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_command(TARGET DendyForgeApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:DendyForgeApp>/assets/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Jura[wght].ttf
        $<TARGET_FILE_DIR:DendyForgeApp>/assets/fonts/Jura[wght].ttf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Jura-OFL.txt
        $<TARGET_FILE_DIR:DendyForgeApp>/assets/fonts/Jura-OFL.txt
)

if(WIN32)
    target_compile_definitions(DendyForgeApp PRIVATE
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
    )
endif()

target_link_libraries(DendyForgeApp PRIVATE
    DendyForgeCore
    SDL3::SDL3
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(DendyForgeApp PRIVATE ws2_32 crypt32)
endif()

add_executable(DendyForgeApuMixerRunner
    tools/apu_mixer_runner.cpp
)

target_link_libraries(DendyForgeApuMixerRunner PRIVATE
    DendyForgeCore
)

add_executable(DendyForgeApuTimingRunner
    tools/apu_timing_runner.cpp
)

target_link_libraries(DendyForgeApuTimingRunner PRIVATE
    DendyForgeCore
)

add_executable(DendyForgePpuRunner
    tools/ppu_runner.cpp
)

target_link_libraries(DendyForgePpuRunner PRIVATE
    DendyForgeCore
)

add_executable(DendyForgeConsoleBenchmark
    tools/console_benchmark.cpp
)

target_link_libraries(DendyForgeConsoleBenchmark PRIVATE
    DendyForgeCore
)

add_executable(DendyForgeAccuracyCoinRunner
    tools/accuracy_coin_runner.cpp
)

target_link_libraries(DendyForgeAccuracyCoinRunner PRIVATE
    DendyForgeCore
)

add_custom_command(TARGET DendyForgeApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3>"
        "$<TARGET_FILE_DIR:DendyForgeApp>"
)

foreach(opensslRuntime OpenSSL::SSL OpenSSL::Crypto)
    add_custom_command(TARGET DendyForgeApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${opensslRuntime}>"
            "$<TARGET_FILE_DIR:DendyForgeApp>"
    )
endforeach()

if(MINGW)
    set(MINGW_BIN_DIR "${DENDYFORGE_MINGW_BIN_DIR}")

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
