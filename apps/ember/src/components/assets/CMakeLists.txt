add_library(ember-assets
        AssetsUpdater.cpp
)
wf_apply_warnings(ember-assets)

target_link_libraries(ember-assets PUBLIC
        squallcore
        squallcurl
        libsigcpp::sigc++
        Boost::headers
)

