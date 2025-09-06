find_package(Catch2 3 REQUIRED)

wf_add_test(conftest.cpp)

add_compile_definitions(SRCDIR="${PROJECT_SOURCE_DIR}/tests")

target_link_libraries(conftest PRIVATE Catch2::Catch2WithMain)
