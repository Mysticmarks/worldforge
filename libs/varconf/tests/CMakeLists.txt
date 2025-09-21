find_package(Catch2 3 REQUIRED)

wf_add_test(conftest.cpp)
wf_add_test(variable_signedness.cpp)
wf_add_test(variable_bool.cpp)

add_compile_definitions(SRCDIR="${PROJECT_SOURCE_DIR}/tests")

target_link_libraries(conftest PRIVATE Catch2::Catch2WithMain)
target_link_libraries(variable_signedness PRIVATE Catch2::Catch2WithMain)
target_link_libraries(variable_bool PRIVATE Catch2::Catch2WithMain)
