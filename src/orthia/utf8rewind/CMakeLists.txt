cmake_minimum_required(VERSION 3.5)
project(utf8rewind)

add_library(${PROJECT_NAME} SHARED
    utf8rewind.c
    unicodedatabase.c
    internal/casemapping.c
    internal/codepoint.c
    internal/composition.c
    internal/database.c
    internal/decomposition.c
    internal/seeking.c
    internal/streaming.c
)

# Expose the directory so consumers get #include "utf8rewind.h" for free
target_include_directories(${PROJECT_NAME} PUBLIC .)

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
