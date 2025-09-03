add_library(ember-lua-console STATIC
        LuaConsoleObject.cpp
)
wf_apply_warnings(ember-lua-console)
target_link_libraries(ember-lua-console PUBLIC
        ember-framework
        ember-external-sol2
)

add_subdirectory(bindings)


add_library(ember-lua_component STATIC
        LuaScriptingProvider.cpp
)
wf_apply_warnings(ember-lua_component)


target_link_libraries(ember-lua_component PUBLIC
        ember-BindingsDomain
        ember-BindingsServices
        ember-BindingsCEGUI
        ember-BindingsAtlasAdapters
        ember-BindingsRepresentations
        ember-BindingsFramework
        ember-BindingsEris
        ember-BindingsVarconf
        ember-BindingsAtlas
        ember-BindingsLua
        ember-BindingsEmberOgre
        ember-BindingsOgre
        ember-BindingsWFMath
        ember-lua-console
        ember-framework
        ember-services
        ember-external-sol2
        eris
)

