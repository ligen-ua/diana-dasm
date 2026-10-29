cmake_minimum_required(VERSION 3.16)
project(orthia_floating_helper_lib LANGUAGES CXX)

# A test library, not installed: orthia_floating_helper loads it and makes it lead into the
# floating code of the helper, for tests/cli/test_floating.py
add_library(${PROJECT_NAME} SHARED main.cpp)

# next to the orthia binary and the helper, which loads it from its own folder; no "lib" prefix,
# so the module has the same name on every system
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    CXX_VISIBILITY_PRESET hidden
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../orthia_disasm_ui"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../orthia_disasm_ui")
