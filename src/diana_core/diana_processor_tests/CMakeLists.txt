project(diana_processor_tests LANGUAGES CXX)

include_directories("../diana_core")
include_directories("../diana_core_tests")

set(FILES_LIST 
main.cpp
test_processor.cpp
test_processor_a.cpp
test_processor_b.cpp
test_processor_c.cpp
test_processor_call.cpp
test_processor_d.cpp
test_processor_flags.cpp
test_processor_fpu.cpp
test_processor_fpu_integration.cpp
test_processor_fpu_sse.cpp
test_processor_i.cpp
test_processor_j.cpp
test_processor_l.cpp
test_processor_m.cpp
test_processor_m_xmm.cpp
test_processor_n.cpp
test_processor_o.cpp
test_processor_r.cpp
test_processor_s.cpp
test_processor_x.cpp
)

add_executable(${PROJECT_NAME} 
    ${FILES_LIST}
)

target_link_libraries(${PROJECT_NAME} diana_core)

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

add_test (NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})