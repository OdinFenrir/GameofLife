cmake_minimum_required(VERSION 3.20)
project(GameOfLife LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gameoflife
    src/main.cpp
    src/Constants.h
    src/Types.h
    src/Simulation.cpp
)

target_include_directories(gameoflife PRIVATE include src)

find_package(SFML COMPONENTS System Window Graphics CONFIG REQUIRED)
target_link_libraries(gameoflife PRIVATE SFML::System SFML::Window SFML::Graphics)

add_custom_command(TARGET gameoflife POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:gameoflife>/assets
)

add_custom_target(run
    COMMAND $<TARGET_FILE:gameoflife>
    DEPENDS gameoflife
    WORKING_DIRECTORY $<TARGET_FILE_DIR:gameoflife>
)
