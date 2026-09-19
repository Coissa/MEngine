add_library(MEngineAssetFormats STATIC
    StaticAssetFormats.cpp
    StaticAssetFormats.h
)

target_include_directories(MEngineAssetFormats PUBLIC ${PROJECT_SOURCE_DIR}/Engine)
target_link_libraries(MEngineAssetFormats PUBLIC MEngineAsset MEngineCook MEngineCore)

add_library(MEngineAssetCookers STATIC
    CookPackagePublisher.cpp
    CookPackagePublisher.h
    CookKey.cpp
    CookKey.h
    StaticAssetCookers.cpp
    StaticAssetCookers.h
)

target_include_directories(MEngineAssetCookers PUBLIC ${PROJECT_SOURCE_DIR}/Engine)
target_link_libraries(MEngineAssetCookers PUBLIC MEngineAssetFormats MEngineCookAuthoring)

if(MSVC)
    target_compile_options(MEngineAssetFormats PRIVATE /EHsc /utf-8)
    target_compile_options(MEngineAssetCookers PRIVATE /EHsc /utf-8)
endif()

add_library(MEngineAssetImporters STATIC
    GltfDependencyResolver.cpp
    GltfDependencyResolver.h
    GltfImporter.cpp
    GltfImporter.h
    TextureImporter.cpp
    TextureImporter.h
)

target_include_directories(MEngineAssetImporters
    PUBLIC ${PROJECT_SOURCE_DIR}/Engine
    PRIVATE ${PROJECT_SOURCE_DIR}/Engine/ThirdParty
)
target_link_libraries(MEngineAssetImporters PUBLIC MEngineAssetFormats MEngineFileSystem)
if(MSVC)
    target_compile_options(MEngineAssetImporters PRIVATE /EHsc /utf-8)
endif()
