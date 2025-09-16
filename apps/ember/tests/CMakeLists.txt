##
## CppUnit-based tests
##
if (TARGET cppunit::cppunit)

    MESSAGE(STATUS "Building tests.")

    add_executable(TestOgreView
            TestOgreView.cpp
            ConvertTestCase.cpp
            ModelMountTestCase.cpp
            NestedEntityTestCase.cpp
            AvatarMovementTestCase.cpp
            InstancingRenderTestCase.cpp
            $<TARGET_OBJECTS:ember-ogre>
            $<TARGET_OBJECTS:ember-caelum>
            $<TARGET_OBJECTS:ember-meshtree>
            $<TARGET_OBJECTS:ember-pagedgeometry>
            $<TARGET_OBJECTS:ember-widgets>
            $<TARGET_OBJECTS:ember-ogre_terrain>
            $<TARGET_OBJECTS:ember-entitymapping>
            $<TARGET_OBJECTS:ember-cegui>
            $<TARGET_OBJECTS:ember-navigation>
            $<TARGET_OBJECTS:ember-services>
            $<TARGET_OBJECTS:ember-domain>
            $<TARGET_OBJECTS:ember-framework>
            $<TARGET_OBJECTS:ember-tinyxml>
            $<TARGET_OBJECTS:binreloc>
            $<TARGET_OBJECTS:CEGUIOgreRenderer>
            $<TARGET_OBJECTS:CEGUITinyXMLParser>
            $<TARGET_OBJECTS:ember-DetourTileCache>
            $<TARGET_OBJECTS:ember-Detour>
            $<TARGET_OBJECTS:ember-Recast>)
    target_compile_definitions(TestOgreView PUBLIC -DLOG_TASKS)
    target_link_libraries(TestOgreView cppunit::cppunit
            Bullet::Bullet
            mojoal)
    add_test(NAME TestOgreView COMMAND TestOgreView)
    add_dependencies(check TestOgreView)

    add_executable(TestTasks
            TestTasks.cpp
            $<TARGET_OBJECTS:ember-framework>
            $<TARGET_OBJECTS:ember-tinyxml>)
    target_compile_definitions(TestTasks PUBLIC -DLOG_TASKS)
    target_link_libraries(TestTasks
            cppunit::cppunit
            mojoal
            eris)
    add_test(NAME TestTasks COMMAND TestTasks)
    add_dependencies(check TestTasks)

    add_executable(TestTimeFrame
            TestTimeFrame.cpp
            $<TARGET_OBJECTS:ember-framework>
            $<TARGET_OBJECTS:ember-tinyxml>)
    target_compile_definitions(TestTimeFrame PUBLIC -DLOG_TASKS)
    target_link_libraries(TestTimeFrame
            cppunit::cppunit
            mojoal)
    add_test(NAME TestTimeFrame COMMAND TestTimeFrame)
    add_dependencies(check TestTimeFrame)

    add_executable(TestFramework
            TestFramework.cpp
            $<TARGET_OBJECTS:ember-framework>
            $<TARGET_OBJECTS:ember-tinyxml>)
    target_compile_definitions(TestFramework PUBLIC -DLOG_TASKS)
    configure_file(atlas.xml atlas.xml COPYONLY)
    target_link_libraries(TestFramework
            cppunit::cppunit
            mojoal
            eris)
    add_test(NAME TestFramework COMMAND TestFramework)
    add_dependencies(check TestFramework)

    add_executable(TestConsoleBackend
            TestConsoleBackend.cpp
            $<TARGET_OBJECTS:ember-framework>
            $<TARGET_OBJECTS:ember-tinyxml>)
    target_compile_definitions(TestConsoleBackend PUBLIC -DLOG_TASKS)
    target_link_libraries(TestConsoleBackend
            cppunit::cppunit
            mojoal)
    add_test(NAME TestConsoleBackend COMMAND TestConsoleBackend)
    add_dependencies(check TestConsoleBackend)

    add_executable(TestEmberServices
            TestEmberServices.cpp)
    target_compile_definitions(TestEmberServices PUBLIC -DLOG_TASKS)
    target_link_libraries(TestEmberServices
            cppunit::cppunit
            ember-services
            mojoal)
    add_test(NAME TestEmberServices COMMAND TestEmberServices)
    add_dependencies(check TestEmberServices)

    add_executable(ComboboxAdapterTest
            ComboboxAdapterTest.cpp)
    target_compile_definitions(ComboboxAdapterTest PUBLIC -DLOG_TASKS)
    target_link_libraries(ComboboxAdapterTest
            cppunit::cppunit
            ember-widgets)
    add_test(NAME ComboboxAdapterTest COMMAND ComboboxAdapterTest)
    add_dependencies(check ComboboxAdapterTest)

    #    add_executable(TestTerrain TestTerrain.cpp)
    #    target_compile_definitions(TestTerrain PUBLIC -DLOG_TASKS)
    #    target_link_libraries(TestTerrain ${CPPUNIT_LIBRARIES} ${WF_LIBRARIES} emberogre terrain caelum pagedgeometry entitymapping lua services framework)
    #    target_include_directories(TestTerrain PUBLIC ${CPPUNIT_INCLUDE_DIRS})
    #    add_test(NAME TestTerrain COMMAND TestTerrain)

endif ()

##
## Catch2-based tests
##
find_package(Catch2 3 REQUIRED)

add_executable(ServerSettingsTest
        ServerSettingsTest.cpp
        ../src/services/serversettings/ServerSettings.cpp
        ../src/services/serversettings/ServerSettingsCredentials.cpp
        ../src/services/config/ConfigService.cpp)
target_include_directories(ServerSettingsTest PRIVATE ../src)
target_link_libraries(ServerSettingsTest Catch2::Catch2WithMain varconf)
add_test(NAME ServerSettingsTest COMMAND ServerSettingsTest)
add_dependencies(check ServerSettingsTest)

add_executable(AutoGraphicsLevelManagerTest
        AutoGraphicsLevelManagerTest.cpp)
target_include_directories(AutoGraphicsLevelManagerTest PRIVATE ../src)
target_link_libraries(AutoGraphicsLevelManagerTest
        Catch2::Catch2WithMain)
add_test(NAME AutoGraphicsLevelManagerTest COMMAND AutoGraphicsLevelManagerTest)
add_dependencies(check AutoGraphicsLevelManagerTest)
