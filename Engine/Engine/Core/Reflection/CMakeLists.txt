add_library(MEngineReflection STATIC
    BuiltinTypeRegistration.cpp
    TypeRegistry.cpp
)

target_include_directories(MEngineReflection
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MEngineReflection PUBLIC MEngineCore)
