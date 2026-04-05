cmake_minimum_required(VERSION 3.5)
project(orthia_ui)

# Create shared library
add_library(${PROJECT_NAME} SHARED 

oui_app.cpp
oui_base.cpp
oui_button.cpp
oui_color.cpp
oui_console.cpp
oui_containers.cpp
oui_editbox.cpp
oui_filesystem.cpp
oui_hotkey.cpp
oui_label.cpp
oui_layouts_calc.cpp
oui_layouts.cpp
oui_listbox.cpp
oui_menu.cpp
oui_modal.cpp
oui_multiline_view.cpp
oui_open_file_dialog.cpp
oui_param.cpp
oui_screen.cpp
oui_scrollable.cpp
oui_string.cpp
oui_text_markup.cpp
oui_threadpool.cpp
oui_window.cpp
oui_window_thread.cpp

)

if(UNIX)
    target_sources(${PROJECT_NAME} PRIVATE
        oui_base_posix.cpp
        oui_console_posix.cpp
        oui_symbols_posix.cpp
    )
    target_link_libraries(${PROJECT_NAME} utf8rewind)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

