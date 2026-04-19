cmake_minimum_required(VERSION 3.5)
project(orthia_disasm_ui)

include_directories("../../diana_core/diana_core")
include_directories("../../diana_core/diana_core/diana_patchers")
include_directories("../../diana_core/diana_core/diana_processor")
include_directories("../orthia_lib")
include_directories("../orthia_ui")

set(SOURCES
    orthia_commands.cpp
    orthia_commands_display.cpp
    orthia_config.cpp
    orthia_helpers.cpp
    orthia_item_file.cpp
    orthia_item_process.cpp
    orthia_model.cpp
    orthia_model_interfaces.cpp
    orthia_model_modules.cpp
    orthia_model_modules_elf.cpp
    orthia_process_adapter.cpp
    oui_disasm_colors.cpp
    oui_goto_dialog.cpp
    oui_open_process_dialog.cpp
    oui_processes.cpp
    ui_command_window.cpp
    ui_common.cpp
    ui_disasm_memory_writer.cpp
    ui_disasm_window.cpp
    ui_en.cpp
    ui_help.cpp
    ui_main_window.cpp
    ui_menu.cpp
    ui_modules_window.cpp
    ui_output_window.cpp
    ui_tests.cpp
    ui_workspace_window.cpp
)

if(UNIX)
    list(APPEND SOURCES
        main_posix.cpp
        oui_processes_posix.cpp
    )
else()
    list(APPEND SOURCES
        main.cpp
        oui_processes_win32.cpp
    )
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

add_library(gtest_lib STATIC ../orthia_ui/gtest/gtest-all.cc)
target_include_directories(gtest_lib PUBLIC ../orthia_ui)

target_link_libraries(${PROJECT_NAME} orthia_lib orthia_ui diana_core utf8rewind gtest_lib)

if(UNIX)
    target_link_libraries(${PROJECT_NAME} pthread)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
