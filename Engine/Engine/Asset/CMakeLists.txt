add_library(MEngineAsset STATIC
    AssetFile.cpp
    AssetFile.h
    AssetId.cpp
    AssetId.h
    AssetKey.cpp
    AssetKey.h
    AssetRegistry.cpp
    AssetRegistry.h
    AssetRegistrySerializer.cpp
    AssetRegistrySerializer.h
    AssetSerialization.cpp
    AssetSerialization.h
    AssetTypeId.h
    ProjectDescriptor.cpp
    ProjectDescriptor.h
)

target_include_directories(MEngineAsset
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineAsset
    PUBLIC
        MEngineCore
        MEngineSerialization
    PRIVATE
        MEngineStduuid
)

if(MSVC)
    target_compile_options(MEngineAsset PRIVATE /EHsc /utf-8)
endif()
