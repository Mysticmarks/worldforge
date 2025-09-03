add_library(ember-ogre-lod STATIC
        LodLevelManager.cpp

        LodDefinition.cpp
        NaniteLodDefinition.cpp
        LodDefinitionManager.cpp
        LodManager.cpp
        XMLLodDefinitionSerializer.cpp
        PMInjectorSignaler.cpp
        ClusterLodStrategy.cpp

)
wf_apply_warnings(ember-ogre-lod)

target_link_libraries(ember-ogre-lod PUBLIC
        ember-framework
        OgreMeshLodGenerator
        OgreMain
)

target_include_directories(ember-ogre-lod PUBLIC .)

