cmake_minimum_required(VERSION 3.16)
project(GameBoy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD  17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED CONFIG)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/core/GameBoy.cpp
) 

target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        VERBATIM
    )
endif()
