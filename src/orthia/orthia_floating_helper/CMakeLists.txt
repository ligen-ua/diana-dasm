cmake_minimum_required(VERSION 3.16)
project(orthia_floating_helper LANGUAGES CXX)

# A test helper, not installed: hooks a function of its own with the diana patcher, for
# tests/cli/test_floating.py
add_executable(${PROJECT_NAME} main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
    ../../diana_core/diana_core
    ../../diana_core/diana_core/diana_patchers)

# dlopen (libdl before glibc 2.34); orthia_floating_helper_lib is loaded at run time, built along
target_link_libraries(${PROJECT_NAME} diana_core ${CMAKE_DL_LIBS})
add_dependencies(${PROJECT_NAME} orthia_floating_helper_lib)

# next to the orthia binary, where the tests look for it
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINKER_LANGUAGE CXX
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../orthia_disasm_ui")
