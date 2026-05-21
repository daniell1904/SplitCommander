cmake_minimum_required(VERSION 3.18)
project(splitcommander-installer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

add_executable(splitcommander-installer installer.cpp)

target_link_libraries(splitcommander-installer PRIVATE
    Qt6::Widgets
    Qt6::Svg
)
