add_library(cyphesis-physics
        Vector3D.cpp
        BBox.cpp
        BBox.h
        Course.cpp
        Quaternion.cpp
        Shape.cpp
        Transform.cpp
        Convert.h
        Course_impl.h
         Shape_impl.h
)
wf_apply_warnings(cyphesis-physics)

target_link_libraries(cyphesis-physics PUBLIC
        wfmath
        Atlas
)

