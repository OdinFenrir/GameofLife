cmake_minimum_required(VERSION 3.20)
project(AntFarm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(antfarm
    src/main.cpp
)

target_include_directories(antfarm PRIVATE include)

find_package(SFML COMPONENTS System Window Graphics CONFIG REQUIRED)
target_link_libraries(antfarm PRIVATE SFML::System SFML::Window SFML::Graphics)

add_custom_target(run
    COMMAND antfarm
    DEPENDS antfarm
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
