add_library(MEngineImGui STATIC
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    backends/imgui_impl_win32.cpp
)

target_include_directories(MEngineImGui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

target_compile_features(MEngineImGui PUBLIC cxx_std_20)

if(MSVC)
    target_compile_definitions(MEngineImGui PRIVATE UNICODE _UNICODE)
    target_compile_options(MEngineImGui PRIVATE /EHsc /utf-8)
    # Piccolo 的这个 ImGui 快照未以 MSVC runtime-check 插桩为目标；
    # 项目代码仍保留 Debug 检查，第三方目标单独关闭。
    set_property(TARGET MEngineImGui PROPERTY MSVC_RUNTIME_CHECKS "")
endif()

target_link_libraries(MEngineImGui PRIVATE user32 imm32)
