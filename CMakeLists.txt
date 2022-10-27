project(diana_dasm)
cmake_minimum_required(VERSION 2.8.2)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

enable_testing()

add_subdirectory(src)
