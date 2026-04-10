cmake_minimum_required(VERSION 3.5)
project(orthia_lib)

include_directories("../../diana_core/diana_core")
include_directories("../../diana_core/diana_core/diana_patchers")
include_directories("../../diana_core/diana_core/diana_processor")
include_directories("../../diana_core/diana_core/diana_processor/softfloat")

# Create shared library
add_library(${PROJECT_NAME} SHARED 

    orthia_common_format.cpp
    orthia_common_print.cpp
    orthia_common_reports.cpp
    orthia_common_time.cpp
    orthia_database_impl_classic.cpp
    orthia_database_impl_vm.cpp
    orthia_database_module.cpp
    orthia_database_saver.cpp
    orthia_diana_module.cpp
    orthia_diana_print.cpp
    orthia_exec.cpp
    orthia_expressions.cpp
    orthia_files_linux.cpp
    orthia_log.cpp
    orthia_memory_cache.cpp
    orthia_module_manager.cpp
    orthia_parser.cpp
    orthia_pe.cpp
    orthia_plugin_interfaces.cpp
    orthia_sha1.c
    orthia_sqlite.cpp
    orthia_stack_analyzer.cpp
    orthia_streams.cpp
    orthia_text_manager.cpp
    orthia_tokenizer.cpp
    orthia_utils.cpp
    orthia_utils_posix.cpp
    orthia_utils_linux.cpp
    orthia_vmlib_api_handlers_common.cpp
    orthia_vmlib_api_handlers.cpp
    orthia_vmlib.cpp
    orthia_vmlib_shuttle.cpp
    orthia_vmlib_utils.cpp
    sqlite3.c
    tinyxml2.cpp

    
)

target_link_libraries(${PROJECT_NAME} utf8rewind)

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

