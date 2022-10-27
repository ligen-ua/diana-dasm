project(diana_core_tests LANGUAGES CXX)

include_directories("../diana_core")

set(FILES_LIST 
main.cpp
test_add.cpp
test_analyze.cpp
test_call.cpp
test_cmp.cpp
test_common.cpp
test_fpu_mmx32.cpp
test_imul.cpp
test_integrated.cpp
test_jmp.cpp
test_lea.cpp
test_masm1.cpp
test_mov.cpp
test_mov2.cpp
test_mov_as.cpp
test_mov_spec.cpp
test_new.cpp
test_or.cpp
test_patchers.cpp
test_push.cpp
test_stack.cpp
test_suxx.cpp
test_x64.cpp
test_x64_integrated.cpp
)

add_executable(${PROJECT_NAME} 
    ${FILES_LIST}
)

target_link_libraries(${PROJECT_NAME} diana_core)

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

add_test (NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})