add_subdirectory(ThirdParty/imgui)

set(MENGINE_SHADER_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Shaders")
set(MENGINE_ASSET_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Assets")
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
file(GLOB_RECURSE MENGINE_SHADER_SOURCES CONFIGURE_DEPENDS
    "${MENGINE_SHADER_SOURCE_DIRECTORY}/*.hlsl"
)
file(GLOB_RECURSE MENGINE_RUNTIME_ASSETS CONFIGURE_DEPENDS
    "${MENGINE_ASSET_SOURCE_DIRECTORY}/*"
)

function(mengine_package_shaders target_name)
    set(package_target "${target_name}ShaderPackage")
    add_custom_target(${package_target}
        COMMAND
            ${CMAKE_COMMAND} -E copy_directory
            "${MENGINE_SHADER_SOURCE_DIRECTORY}"
            "$<TARGET_FILE_DIR:${target_name}>/Shaders"
        DEPENDS
            ${MENGINE_SHADER_SOURCES}
        SOURCES
            ${MENGINE_SHADER_SOURCES}
        COMMENT
            "Packaging HLSL shaders for ${target_name}"
        VERBATIM
    )
    add_dependencies(${target_name} ${package_target})
endfunction()

function(mengine_package_assets target_name)
    set(package_target "${target_name}AssetPackage")
    set(packaged_authoring_files "")
    foreach(relative_path IN LISTS MENGINE_GENERATED_SKYBOX_NON_RUNTIME_FILES)
        list(APPEND packaged_authoring_files
            "$<TARGET_FILE_DIR:${target_name}>/Assets/${relative_path}"
        )
    endforeach()
    add_custom_target(${package_target}
        COMMAND
            ${CMAKE_COMMAND} -E copy_directory
            "${MENGINE_ASSET_SOURCE_DIRECTORY}"
            "$<TARGET_FILE_DIR:${target_name}>/Assets"
        # 制作 PNG 与来源说明不是运行时 Cubemap 输入。
        COMMAND
            ${CMAKE_COMMAND} -E rm -f
            ${packaged_authoring_files}
        # Piccolo 资源只作为仓库中的历史来源保留，不再进入运行时包。
        COMMAND
            ${CMAKE_COMMAND} -E remove_directory
            "$<TARGET_FILE_DIR:${target_name}>/Assets/Textures/Sky/Piccolo"
        DEPENDS
            ${MENGINE_RUNTIME_ASSETS}
        SOURCES
            ${MENGINE_RUNTIME_ASSETS}
        COMMENT
            "Packaging runtime assets for ${target_name}"
        VERBATIM
    )
    add_dependencies(${target_name} ${package_target})
endfunction()

add_subdirectory(Engine/Core)
add_subdirectory(Engine/Core/Reflection)
add_subdirectory(Engine/RHI)
add_subdirectory(Engine/RHI/D3D12)
add_subdirectory(Engine/RenderCore)
add_subdirectory(Engine/Renderer)
add_subdirectory(Engine/Platform)
add_subdirectory(Engine/Runtime)
add_subdirectory(Engine/Launch)
add_subdirectory(Game)
add_subdirectory(Editor)

if(BUILD_TESTING)
    add_subdirectory(Tests/Smoke)
endif()
