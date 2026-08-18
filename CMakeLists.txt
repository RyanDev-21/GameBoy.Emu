cmake_minimum_required(VERSION 3.16)
project(GameBoy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD  17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_library(imgui STATIC 
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_impl_sdl2.cpp
    imgui/imgui_impl_sdlrenderer2.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
)
target_include_directories(imgui PUBLIC imgui
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(imgui PUBLIC 
    SDL2::SDL2
)
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/core/GameBoy.cpp
    src/core/cpu.cpp
    src/core/ppu.cpp
    src/core/bus.cpp
    src/core/timer.cpp
    src/core/interrupt.cpp
    src/core/joypad.cpp
    src/core/apu/apu.cpp
    src/core/apu/channels/noiseChannel.cpp
    src/core/apu/channels/squareChannel.cpp
    src/core/apu/channels/waveChannel.cpp
    src/utils/Debug.cpp
    src/utils/StringUtils.cpp
    src/Platform/Platform.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE 
    src
    ${SDL2_INCLUDE_DIRS}
)

# 3. Link Executable Dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE 
    imgui
    SDL2::SDL2
)
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        VERBATIM
    )
endif()
