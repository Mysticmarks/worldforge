include(FetchContent)

find_package(Catch2 3 QUIET)

if (NOT Catch2_FOUND)
    message(STATUS "Catch2 3 not found; fetching via FetchContent")
    FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.2
    )
    FetchContent_MakeAvailable(Catch2)
endif ()

if (NOT TARGET Catch2::Catch2WithMain)
    message(FATAL_ERROR "Catch2::Catch2WithMain target is unavailable")
endif ()

function(wf_add_catch_test TEST_FILE)
    wf_add_test(${TEST_FILE} ${ARGN})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    target_link_libraries(${TEST_NAME} Catch2::Catch2WithMain)
endfunction()

# Add test
wf_add_catch_test(ball_test.cpp)
wf_add_catch_test(const_test.cpp)
wf_add_catch_test(intstring_test.cpp)
wf_add_catch_test(line_test.cpp)
wf_add_catch_test(point_test.cpp)
wf_add_catch_test(polygon_test.cpp)
wf_add_catch_test(probability_test.cpp)
wf_add_catch_test(quaternion_test.cpp)
wf_add_catch_test(randgen_test.cpp)
wf_add_catch_test(rotmatrix_test.cpp)
wf_add_catch_test(shape_test.cpp)
wf_add_catch_test(timestamp_test.cpp)
wf_add_catch_test(vector_test.cpp)
wf_add_catch_test(intersect_unknown_test.cpp)
wf_add_catch_test(barycenter_test.cpp)

if (Atlas_FOUND)
    link_directories(${Atlas_LIBRARY_DIR})
    wf_add_catch_test(atlas/atlas_0_7_test.cpp)
    target_link_libraries(atlas_0_7_test ${Atlas_LIBRARIES})
    target_include_directories(atlas_0_7_test PUBLIC ${Atlas_INCLUDE_DIR})
endif (Atlas_FOUND)
