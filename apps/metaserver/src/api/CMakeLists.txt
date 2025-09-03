add_library(metaserver-api
        MetaServerPacket.cpp
)
wf_apply_warnings(metaserver-api)

target_include_directories(metaserver-api PUBLIC .)

