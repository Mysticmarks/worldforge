wf_find_boost(program_options)
find_package(LLVM REQUIRED CONFIG)

add_executable(metaserver
        main.cpp
        MetaServer.cpp
        MetaServerHandlerUDP.cpp
        DataObject.cpp
)
target_include_directories(metaserver PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(metaserver
        metaserver-api
        Boost::program_options
        spdlog::spdlog
        LLVM::Support
)
install(TARGETS metaserver DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})


add_executable(ms-testserver
        TestServer.cpp
)
target_link_libraries(ms-testserver
        metaserver-api
        Boost::program_options
        spdlog::spdlog
)


add_executable(ms-testclient
        TestClient.cpp
)
target_link_libraries(ms-testclient
        metaserver-api
        Boost::program_options
        spdlog::spdlog
)


add_executable(ms-pdnspipe
        PDNSPipe.cpp
)
target_link_libraries(ms-pdnspipe
        metaserver-api
        Boost::program_options
        spdlog::spdlog
)
