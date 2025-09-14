add_library(external-Detour
        Source/DetourAlloc.cpp
        Source/DetourAssert.cpp
        Source/DetourCommon.cpp
        Source/DetourNavMesh.cpp
        Source/DetourNavMeshBuilder.cpp
        Source/DetourNavMeshQuery.cpp
        Source/DetourNode.cpp
)

target_include_directories(external-Detour PUBLIC Include)

if(BUILD_TESTING)
    enable_testing()
    add_executable(DetourNavMeshBuilderTest tests/DetourNavMeshBuilderTest.cpp)
    target_link_libraries(DetourNavMeshBuilderTest external-Detour)
    add_test(NAME DetourNavMeshBuilderTest COMMAND DetourNavMeshBuilderTest)
endif()
