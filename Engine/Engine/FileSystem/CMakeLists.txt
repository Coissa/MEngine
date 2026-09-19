add_library(MEngineFileSystem STATIC
    FileMount.cpp
    FileMount.h
    LogicalPath.cpp
    LogicalPath.h
)

target_include_directories(MEngineFileSystem
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineFileSystem
    PUBLIC
        MEngineCore
)

if(MSVC)
    target_compile_options(MEngineFileSystem PRIVATE /EHsc /utf-8)
endif()

add_library(MEngineFileSystemAuthoring STATIC
    AtomicFilePublisher.cpp
    AtomicFilePublisher.h
)

target_include_directories(MEngineFileSystemAuthoring
    PUBLIC
        ${PROJECT_SOURCE_DIR}/Engine
)

target_link_libraries(MEngineFileSystemAuthoring
    PUBLIC
        MEngineCore
)

if(MSVC)
    target_compile_options(MEngineFileSystemAuthoring PRIVATE /EHsc /utf-8)
endif()
