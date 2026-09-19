add_library(MEngineStduuid INTERFACE)

target_include_directories(MEngineStduuid
    SYSTEM
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
