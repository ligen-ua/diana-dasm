cmake_minimum_required(VERSION 3.5)
project(diana_dasm)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

enable_testing()

add_subdirectory(src)
