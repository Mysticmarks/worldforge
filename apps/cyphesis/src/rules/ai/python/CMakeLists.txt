add_library(cyphesis-rulesai_python
        CyPy_BaseMind.cpp
        CyPy_MemMap.cpp
        CyPy_Ai.cpp
        CyPy_Steering.cpp
        CyPy_MemEntity.cpp)
wf_apply_warnings(cyphesis-rulesai_python)

target_link_libraries(cyphesis-rulesai_python PUBLIC
        cyphesis-rulesai
        cyphesis-pythonbase)



