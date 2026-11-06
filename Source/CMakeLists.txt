file(GLOB_RECURSE engine_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
list(REMOVE_ITEM engine_sources "${CMAKE_CURRENT_SOURCE_DIR}/Editor/EditorMain.cpp")
add_library(Engine STATIC ${engine_sources})
target_compile_features(Engine PUBLIC cxx_std_20)
target_include_directories(Engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/Core/Reflection"
    PRIVATE "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/ThirdParty")
target_compile_definitions(Engine PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(Engine PRIVATE /EHsc /MP)
target_link_libraries(Engine PRIVATE d3d12 dxgi user32 imm32 comdlg32)
add_subdirectory("${PROJECT_SOURCE_DIR}/ThirdParty/imgui" "${CMAKE_CURRENT_BINARY_DIR}/ThirdParty/imgui")
add_subdirectory("${PROJECT_SOURCE_DIR}/ThirdParty/stduuid" "${CMAKE_CURRENT_BINARY_DIR}/ThirdParty/stduuid")
set(CMAKE_FOLDER "")
set_target_properties(Engine PROPERTIES FOLDER "")
add_executable(MEngineEditor Editor/EditorMain.cpp)
target_link_libraries(MEngineEditor PRIVATE Engine)
target_compile_options(MEngineEditor PRIVATE /EHsc)
set_target_properties(MEngineEditor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
include("${PROJECT_SOURCE_DIR}/cmake/Shaders.cmake")

file(GLOB_RECURSE runtime_assets CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/Assets/*")
set(MENGINE_GENERATED_SKYBOX_NON_RUNTIME_FILES
    "Textures/Sky/GeneratedDayBlack_v1/skybox_source_generated.png"
    "Textures/Sky/GeneratedDayBlack_v1/skybox_equirectangular.png"
    "Textures/Sky/GeneratedDayBlack_v1/skybox_cube_cross_preview.png"
    "Textures/Sky/GeneratedDayBlack_v1/skybox_X+.png"
    "Textures/Sky/GeneratedDayBlack_v1/skybox_X-.png"
    "Textures/Sky/GeneratedDayBlack_v1/skybox_Y+.png"
    "Textures/Sky/GeneratedDayBlack_v1/skybox_Y-.png"
    "Textures/Sky/GeneratedDayBlack_v1/skybox_Z+.png"
    "Textures/Sky/GeneratedDayBlack_v1/skybox_Z-.png"
    "Textures/Sky/GeneratedDayBlack_v1/SOURCE.md"
)
set(packaged_authoring_files "")
foreach(path IN LISTS MENGINE_GENERATED_SKYBOX_NON_RUNTIME_FILES)
    list(APPEND packaged_authoring_files "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Assets/${path}")
endforeach()
set(asset_stamp "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Assets.stamp")
add_custom_command(OUTPUT "${asset_stamp}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/Assets" "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Assets"
    COMMAND ${CMAKE_COMMAND} -E rm -f ${packaged_authoring_files}
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Assets/Textures/Sky/Piccolo"
    COMMAND ${CMAKE_COMMAND} -E touch "${asset_stamp}"
    DEPENDS ${runtime_assets} VERBATIM)
target_sources(MEngineEditor PRIVATE "${asset_stamp}")
get_target_property(all_engine_sources Engine SOURCES)
list(FILTER all_engine_sources EXCLUDE REGEX "\\.cso$")
source_group(TREE "${PROJECT_SOURCE_DIR}" FILES ${all_engine_sources})
set_property(TARGET Engine PROPERTY MSVC_RUNTIME_CHECKS "")
set_source_files_properties(${engine_sources} PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Debug>:/RTC1>")

source_group("Shaders\\Bytecode" REGULAR_EXPRESSION "\\.cso$")
