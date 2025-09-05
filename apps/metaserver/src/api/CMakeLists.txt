wf_find_boost(system headers)

add_library(metaserver-api
        MetaServerPacket.cpp
        MetaServerAPI.cpp
)

target_link_libraries(metaserver-api PUBLIC
        Boost::headers
        Boost::system
)

target_include_directories(metaserver-api PUBLIC .)
