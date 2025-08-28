include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR})
add_executable(MeshPreprocessorTest MeshPreprocessorTest.cpp)
add_test(NAME MeshPreprocessorTest COMMAND MeshPreprocessorTest)
