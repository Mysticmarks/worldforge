add_library(ember-ogre-mapping STATIC
        EmberEntityMappingManager.cpp
        ModelActionCreator.cpp
        XMLEntityMappingDefinitionSerializer.cpp
)
wf_apply_warnings(ember-ogre-mapping)

target_link_libraries(ember-ogre-mapping PUBLIC
        ember-ogre-support
        ember-framework
        ember-entitymapping
        OgreMain
)

target_include_directories(ember-ogre-mapping PUBLIC .)

