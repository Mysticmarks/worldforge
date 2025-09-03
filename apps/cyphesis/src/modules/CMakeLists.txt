add_library(cyphesis-modules
        DateTime.cpp
        WorldTime.cpp)
wf_apply_warnings(cyphesis-modules)


target_link_libraries(cyphesis-modules PUBLIC
        cyphesis-common
)
