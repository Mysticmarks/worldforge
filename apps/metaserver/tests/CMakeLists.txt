include_directories(../src/server ../src/api)
find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})

if (BUILD_METASERVER_SERVER)

    wf_find_boost(program_options)

    add_executable(MetaServer_unittest
            MetaServer_unittest.cpp
            ../src/server/MetaServer.cpp
            ../src/api/MetaServerPacket.cpp
            ../src/server/DataObject.cpp)
    target_link_libraries(MetaServer_unittest PUBLIC
            cppunit::cppunit
            Boost::program_options
            spdlog::spdlog
            LLVM::Support
    )
    add_test(NAME MetaServer_unittest COMMAND MetaServer_unittest)
    add_dependencies(check MetaServer_unittest)


    add_executable(MetaServerPackage_unittest
            MetaServerPacket_unittest.cpp
            ../src/api/MetaServerPacket.cpp)
    target_link_libraries(MetaServerPackage_unittest PUBLIC
            cppunit::cppunit
            Boost::program_options
            spdlog::spdlog
    )
    add_test(NAME MetaServerPackage_unittest COMMAND MetaServerPackage_unittest)
    add_dependencies(check MetaServerPackage_unittest)


    add_executable(DataObject_unittest
            DataObject_unittest.cpp
            ../src/server/DataObject.cpp)
    target_link_libraries(DataObject_unittest PUBLIC
            cppunit::cppunit
            Boost::program_options
            spdlog::spdlog
    )
    add_test(NAME DataObject_unittest COMMAND DataObject_unittest)
    add_dependencies(check DataObject_unittest)


    add_executable(ClientSessionCache_unittest
            ClientSessionCache_unittest.cpp
            ../src/server/DataObject.cpp)
    target_link_libraries(ClientSessionCache_unittest PUBLIC
            cppunit::cppunit
            Boost::program_options
            spdlog::spdlog
    )
    add_test(NAME ClientSessionCache_unittest COMMAND ClientSessionCache_unittest)
    add_dependencies(check ClientSessionCache_unittest)


    add_executable(MetaServerHandlerUDP_unittest
            MetaServerHandlerUDP_unittest.cpp
            ../src/server/MetaServerHandlerUDP.cpp
            ../src/api/MetaServerPacket.cpp)
    target_link_libraries(MetaServerHandlerUDP_unittest PUBLIC
            cppunit::cppunit
            Boost::program_options
            spdlog::spdlog
    )
    add_test(NAME MetaServerHandlerUDP_unittest COMMAND MetaServerHandlerUDP_unittest)
    add_dependencies(check MetaServerHandlerUDP_unittest)

    add_executable(DomainFilter_unittest
            DomainFilter_unittest.cpp)
    target_link_libraries(DomainFilter_unittest PUBLIC
            cppunit::cppunit
            Boost::program_options
            spdlog::spdlog
    )
    add_test(NAME DomainFilter_unittest COMMAND DomainFilter_unittest)
    add_dependencies(check DomainFilter_unittest)
endif ()
