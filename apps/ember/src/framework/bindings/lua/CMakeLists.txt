add_subdirectory(atlas)
add_subdirectory(eris)
add_subdirectory(varconf)
add_subdirectory(wfmath)

add_library(ember-BindingsFramework STATIC
        BindingsFramework.cpp
)
wf_apply_warnings(ember-BindingsFramework)
target_link_libraries(ember-BindingsFramework PUBLIC
        ember-framework
        ember-external-sol2
)

target_precompile_headers(ember-BindingsFramework
        REUSE_FROM
        ember-BindingsLua
)
if (MSVC)
    #Need to allow for a lot of symbols when compiling the bindings on MSVC.
    target_compile_options(ember-BindingsFramework PRIVATE "/bigobj")
endif ()