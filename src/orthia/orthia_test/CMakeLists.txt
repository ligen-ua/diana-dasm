cmake_minimum_required(VERSION 3.5)
project(orthia_test LANGUAGES CXX)


include_directories("../../diana_core/diana_core")
include_directories("../../diana_core/diana_core/diana_patchers")
include_directories("../../diana_core/diana_core/diana_processor")
include_directories("../../diana_core/diana_core/diana_processor/softfloat")
include_directories("../orthia_lib")
include_directories("../../diana_core/diana_core_tests")

set(FILES_LIST 
main.cpp
../../diana_core/diana_core_tests/test_common.cpp
ot_cache_test.cpp  
ot_expressions.cpp     
ot_tokenizer.cpp  
ot_vm_test.cpp
ot_memory_manager.cpp  
ot_utils.cpp
ot_elf_test.cpp
)

add_executable(${PROJECT_NAME} 
    ${FILES_LIST}
)

target_link_libraries(${PROJECT_NAME} diana_core orthia_lib)

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

add_test (NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})