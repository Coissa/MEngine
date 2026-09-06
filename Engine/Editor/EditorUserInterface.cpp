#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "EditorUserInterface.h"

#include "EditorCommand.h"
#include "EditorEngine.h"

#include <Engine/Runtime/GameObject.h>
#include <Engine/Runtime/World.h>
#include <Engine/Runtime/WorldContext.h>

#include <Windows.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window, UINT message, WPARAM word_parameter, LPARAM long_parameter);

namespace GE::Editor
{
    namespace
    {
        constexpr std::uintptr_t FontTextureId = 1;
        constexpr std::uintptr_t SceneViewportTextureId = 2;

        template <std::size_t Size>
        void CopyToBuffer(const std::string& source, std::array<char, Size>& buffer)
        {
            const std::size_t count = (std::min)(source.size(), Size - 1);
            std::memcpy(buffer.data(), source.data(), count);
            buffer[count] = '\0';
        }

        void Queue(EditorEngine& editor, EditorCommand command)
        {
            (void)editor.QueueCommand(std::move(command));
        }

        void BuildEditorDockspace()
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
            ImGui::SetNextWindowViewport(viewport->ID);

            constexpr ImGuiWindowFlags HostWindowFlags =
                ImGuiWindowFlags_NoTitleBar
                | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoDocking
                | ImGuiWindowFlags_NoBringToFrontOnFocus
                | ImGuiWindowFlags_NoNavFocus;
            constexpr ImGuiDockNodeFlags DockspaceFlags =
                ImGuiDockNodeFlags_DockSpace;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
            ImGui::Begin("MEngine Dockspace Host", nullptr, HostWindowFlags);
            ImGui::PopStyleVar(3);

            const ImGuiID dockspace = ImGui::GetID("MEngine Main Dockspace");
            if (ImGui::DockBuilderGetNode(dockspace) == nullptr)
            {
                ImGui::DockBuilderRemoveNode(dockspace);
                ImGui::DockBuilderAddNode(dockspace, DockspaceFlags);
                ImGui::DockBuilderSetNodePos(dockspace, viewport->WorkPos);
                ImGui::DockBuilderSetNodeSize(dockspace, viewport->WorkSize);

                ImGuiID main_area = dockspace;
                const ImGuiID inspector = ImGui::DockBuilderSplitNode(
                    main_area, ImGuiDir_Right, 0.25F, nullptr, &main_area);

                ImGuiID upper_area = main_area;
                ImGuiID bottom_area = ImGui::DockBuilderSplitNode(
                    upper_area, ImGuiDir_Down, 0.30F, nullptr, &upper_area);

                ImGuiID viewport_area = upper_area;
                const ImGuiID hierarchy = ImGui::DockBuilderSplitNode(
                    viewport_area, ImGuiDir_Left, 0.30F, nullptr, &viewport_area);

                ImGuiID scene_area = bottom_area;
                const ImGuiID play_area = ImGui::DockBuilderSplitNode(
                    scene_area, ImGuiDir_Right, 0.24F, nullptr, &scene_area);

                ImGui::DockBuilderDockWindow("Hierarchy", hierarchy);
                ImGui::DockBuilderDockWindow("Viewport", viewport_area);
                ImGui::DockBuilderDockWindow("Inspector", inspector);
                ImGui::DockBuilderDockWindow("Scene", scene_area);
                ImGui::DockBuilderDockWindow("Play", play_area);
                ImGui::DockBuilderFinish(dockspace);
            }

            ImGui::DockSpace(dockspace, ImVec2{});
            ImGui::End();
        }
    }

    EditorUserInterface::~EditorUserInterface() noexcept
    {
        Shutdown();
    }

    bool EditorUserInterface::Initialize(
        std::uintptr_t native_window,
        EditorFontAtlas& out_font_atlas,
        std::string& out_error)
    {
        out_font_atlas = {};
        out_error.clear();
        if (m_initialized || native_window == 0)
        {
            out_error = "Editor UI received an invalid initialization request";
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGuiContext* context = ImGui::CreateContext();
        if (context == nullptr)
        {
            out_error = "ImGui context creation failed";
            return false;
        }
        m_context = context;
        ImGui::SetCurrentContext(context);
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
        io.BackendRendererName = "MEngine value-packet UI renderer";
        ImGui::StyleColorsDark();

        if (!ImGui_ImplWin32_Init(reinterpret_cast<HWND>(native_window)))
        {
            out_error = "ImGui Win32 backend initialization failed";
            Shutdown();
            return false;
        }

        unsigned char* pixels = nullptr;
        int width = 0;
        int height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        if (pixels == nullptr || width <= 0 || height <= 0)
        {
            out_error = "ImGui font atlas generation failed";
            Shutdown();
            return false;
        }
        try
        {
            const std::size_t byte_count = static_cast<std::size_t>(width)
                * static_cast<std::size_t>(height) * 4;
            out_font_atlas.width = static_cast<std::uint32_t>(width);
            out_font_atlas.height = static_cast<std::uint32_t>(height);
            out_font_atlas.rgba_pixels.resize(byte_count);
            std::memcpy(out_font_atlas.rgba_pixels.data(), pixels, byte_count);
        }
        catch (...)
        {
            out_font_atlas = {};
            out_error = "Could not copy the ImGui font atlas";
            Shutdown();
            return false;
        }
        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(FontTextureId));
        m_initialized = true;
        return true;
    }

    bool EditorUserInterface::BuildFrame(
        EditorEngine& editor,
        GE::RenderCore::RenderFrame& out_frame) noexcept
    {
        if (!m_initialized || m_context == nullptr)
        {
            return true;
        }
        try
        {
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_context));
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("New"))
                    {
                        Queue(editor, {.type = EditorCommandType::NewScene});
                    }
                    if (ImGui::MenuItem("Open"))
                    {
                        EditorCommand command{.type = EditorCommandType::LoadScene};
                        command.path = m_scene_path;
                        Queue(editor, std::move(command));
                    }
                    if (ImGui::MenuItem("Save"))
                    {
                        EditorCommand command{.type = EditorCommandType::SaveScene};
                        command.path = m_scene_path;
                        Queue(editor, std::move(command));
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }

            BuildEditorDockspace();

            ImGui::Begin("Scene");
            std::array<char, 512> scene_path{};
            CopyToBuffer(m_scene_path, scene_path);
            if (ImGui::InputText("Path", scene_path.data(), scene_path.size()))
            {
                m_scene_path = scene_path.data();
            }
            if (ImGui::Button("New"))
            {
                Queue(editor, {.type = EditorCommandType::NewScene});
            }
            ImGui::SameLine();
            if (ImGui::Button("Open"))
            {
                EditorCommand command{.type = EditorCommandType::LoadScene};
                command.path = m_scene_path;
                Queue(editor, std::move(command));
            }
            ImGui::SameLine();
            if (ImGui::Button("Save"))
            {
                EditorCommand command{.type = EditorCommandType::SaveScene};
                command.path = m_scene_path;
                Queue(editor, std::move(command));
            }
            if (!editor.GetWorkflowStatus().empty())
            {
                ImGui::TextWrapped("%s", editor.GetWorkflowStatus().c_str());
            }
            ImGui::End();

            ImGui::Begin("Play");
            if (!editor.IsPlaySessionRunning())
            {
                if (ImGui::Button("Play"))
                {
                    Queue(editor, {.type = EditorCommandType::StartPlay});
                }
            }
            else if (ImGui::Button("Stop"))
            {
                Queue(editor, {.type = EditorCommandType::StopPlay});
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(editor.IsPlaySessionRunning()
                ? "PIE is running (editing locked)" : "Edit mode");
            ImGui::End();

            GE::Runtime::WorldContext* context = editor.IsPlaySessionRunning()
                ? editor.GetPIEWorldContext() : editor.GetEditorWorldContext();
            GE::Runtime::World* world = context == nullptr
                ? nullptr : context->GetWorld();

            ImGui::Begin("Viewport");
            ImVec2 available = ImGui::GetContentRegionAvail();
            available.x = (std::max)(available.x, 1.0F);
            available.y = (std::max)(available.y, 1.0F);
            out_frame.scene_extent = {
                .width = static_cast<std::uint32_t>(available.x),
                .height = static_cast<std::uint32_t>(available.y)};
            ImGui::Image(
                reinterpret_cast<ImTextureID>(SceneViewportTextureId), available);

            const bool edit_mode = !editor.IsPlaySessionRunning();
            const bool image_hovered = ImGui::IsItemHovered();
            if (!edit_mode)
            {
                m_viewport_navigation_active = false;
            }
            else
            {
                if (image_hovered
                    && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                {
                    m_viewport_navigation_active = true;
                }
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
                {
                    m_viewport_navigation_active = false;
                }

                if (m_viewport_navigation_active)
                {
                    const ImGuiIO& io = ImGui::GetIO();
                    EditorCameraInput input{
                        .forward_axis =
                            (ImGui::IsKeyDown(ImGuiKey_W) ? 1.0F : 0.0F)
                            - (ImGui::IsKeyDown(ImGuiKey_S) ? 1.0F : 0.0F),
                        .right_axis =
                            (ImGui::IsKeyDown(ImGuiKey_D) ? 1.0F : 0.0F)
                            - (ImGui::IsKeyDown(ImGuiKey_A) ? 1.0F : 0.0F),
                        .up_axis =
                            (ImGui::IsKeyDown(ImGuiKey_Q) ? 1.0F : 0.0F)
                            - (ImGui::IsKeyDown(ImGuiKey_E) ? 1.0F : 0.0F),
                        .mouse_delta_x = io.MouseDelta.x,
                        .mouse_delta_y = io.MouseDelta.y,
                        .wheel_delta = io.MouseWheel};
                    editor.UpdateEditorCamera(input, io.DeltaTime);
                    ImGui::SetMouseCursor(ImGuiMouseCursor_None);
                }

                if (!m_viewport_navigation_active
                    && ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    const ImVec2 image_min = ImGui::GetItemRectMin();
                    const ImVec2 image_max = ImGui::GetItemRectMax();
                    const ImVec2 mouse = ImGui::GetMousePos();
                    const float width = image_max.x - image_min.x;
                    const float height = image_max.y - image_min.y;
                    if (width > 0.0F && height > 0.0F)
                    {
                        editor.PickEditorObject(
                            {(mouse.x - image_min.x) / width,
                             (mouse.y - image_min.y) / height},
                            out_frame.scene_extent);
                    }
                }
            }
            ImGui::End();

            ImGui::Begin("Hierarchy");
            if (!editor.IsPlaySessionRunning() && ImGui::Button("Add Object"))
            {
                EditorCommand command{.type = EditorCommandType::CreateObject};
                command.name = "GameObject";
                Queue(editor, std::move(command));
            }
            if (world != nullptr)
            {
                for (GE::Runtime::GameObjectHandle handle
                    : world->GetGameObjectHandles())
                {
                    const GE::Runtime::GameObject* object =
                        world->FindGameObject(handle);
                    if (object == nullptr)
                    {
                        continue;
                    }
                    ImGui::PushID(static_cast<int>(handle.index));
                    ImGui::PushID(static_cast<int>(handle.generation));
                    if (ImGui::Selectable(
                            object->GetName().c_str(),
                            object->GetId() == editor.GetSelectedObjectId()))
                    {
                        (void)editor.SelectObject(object->GetId());
                    }
                    ImGui::PopID();
                    ImGui::PopID();
                }
            }
            ImGui::End();

            ImGui::Begin("Inspector");
            GE::Runtime::GameObject* selected = world == nullptr
                ? nullptr : world->FindGameObjectById(
                    editor.GetSelectedObjectId());
            if (selected == nullptr)
            {
                editor.ClearSelection();
                ImGui::TextUnformatted("Select an object in the Hierarchy.");
            }
            else
            {
                ImGui::Text("ID: %llu",
                    static_cast<unsigned long long>(selected->GetId().value));
                std::array<char, 256> name{};
                CopyToBuffer(selected->GetName(), name);
                if (!editor.IsPlaySessionRunning()
                    && ImGui::InputText("Name", name.data(), name.size()))
                {
                    EditorCommand command{.type = EditorCommandType::RenameObject};
                    command.target = selected->GetHandle();
                    command.name = name.data();
                    Queue(editor, std::move(command));
                }

                GE::Runtime::TransformComponent transform =
                    selected->GetTransform();
                bool transform_changed = false;
                transform_changed |= ImGui::DragFloat3(
                    "Translation", transform.translation.data(), 0.05F);
                transform_changed |= ImGui::DragFloat3(
                    "Rotation", transform.rotation_degrees.data(), 0.25F);
                transform_changed |= ImGui::DragFloat3(
                    "Scale", transform.scale.data(), 0.02F, 0.001F, 1000.0F);
                if (!editor.IsPlaySessionRunning() && transform_changed)
                {
                    EditorCommand command{.type = EditorCommandType::SetTransform};
                    command.target = selected->GetHandle();
                    command.transform = transform;
                    Queue(editor, std::move(command));
                }

                if (GE::Runtime::MeshRendererComponent* mesh =
                    selected->GetMeshRenderer())
                {
                    ImGui::Separator();
                    ImGui::TextUnformatted("Mesh Renderer");
                    int mesh_id = static_cast<int>(mesh->mesh.value);
                    int material_id = static_cast<int>(mesh->material.value);
                    bool visible = mesh->visible;
                    const bool changed =
                        ImGui::InputInt("Mesh", &mesh_id)
                        | ImGui::InputInt("Material", &material_id)
                        | ImGui::Checkbox("Visible", &visible);
                    if (!editor.IsPlaySessionRunning() && changed
                        && mesh_id > 0 && material_id > 0)
                    {
                        EditorCommand command{
                            .type = EditorCommandType::SetMeshRenderer};
                        command.target = selected->GetHandle();
                        command.mesh_renderer = *mesh;
                        command.mesh_renderer.mesh.value =
                            static_cast<std::uint32_t>(mesh_id);
                        command.mesh_renderer.material.value =
                            static_cast<std::uint32_t>(material_id);
                        command.mesh_renderer.visible = visible;
                        Queue(editor, std::move(command));
                    }
                    if (!editor.IsPlaySessionRunning()
                        && ImGui::Button("Remove Mesh Renderer"))
                    {
                        Queue(editor, {
                            .type = EditorCommandType::RemoveMeshRenderer,
                            .target = selected->GetHandle()});
                    }
                }
                else if (!editor.IsPlaySessionRunning()
                    && ImGui::Button("Add Mesh Renderer"))
                {
                    EditorCommand command{
                        .type = EditorCommandType::SetMeshRenderer};
                    command.target = selected->GetHandle();
                    command.mesh_renderer.mesh.value = 1;
                    command.mesh_renderer.material.value = 1;
                    Queue(editor, std::move(command));
                }

                if (GE::Runtime::CameraComponent* camera = selected->GetCamera())
                {
                    ImGui::Separator();
                    ImGui::TextUnformatted("Camera");
                    GE::Runtime::CameraComponent value = *camera;
                    const bool changed =
                        ImGui::DragFloat("Vertical FOV", &value.vertical_field_of_view_degrees,
                            0.25F, 1.0F, 179.0F)
                        | ImGui::DragFloat("Near", &value.near_plane,
                            0.01F, 0.001F, 1000.0F)
                        | ImGui::DragFloat("Far", &value.far_plane,
                            1.0F, 0.01F, 100000.0F)
                        | ImGui::Checkbox("Primary", &value.primary);
                    if (!editor.IsPlaySessionRunning() && changed)
                    {
                        value.vertical_field_of_view_degrees = std::clamp(
                            value.vertical_field_of_view_degrees, 1.0F, 179.0F);
                        value.near_plane = std::max(value.near_plane, 0.001F);
                        value.far_plane = std::max(
                            value.far_plane, value.near_plane + 0.001F);
                        EditorCommand command{.type = EditorCommandType::SetCamera};
                        command.target = selected->GetHandle();
                        command.camera = value;
                        Queue(editor, std::move(command));
                    }
                    if (!editor.IsPlaySessionRunning()
                        && ImGui::Button("Remove Camera"))
                    {
                        Queue(editor, {
                            .type = EditorCommandType::RemoveCamera,
                            .target = selected->GetHandle()});
                    }
                }
                else if (!editor.IsPlaySessionRunning()
                    && ImGui::Button("Add Camera"))
                {
                    EditorCommand command{.type = EditorCommandType::SetCamera};
                    command.target = selected->GetHandle();
                    Queue(editor, std::move(command));
                }

                if (!editor.IsPlaySessionRunning())
                {
                    ImGui::Separator();
                    if (ImGui::Button("Delete Object"))
                    {
                        Queue(editor, {
                            .type = EditorCommandType::DeleteObject,
                            .target = selected->GetHandle()});
                    }
                }
            }
            ImGui::End();

            ImGui::Render();
            const ImDrawData* draw_data = ImGui::GetDrawData();
            out_frame.ui = {};
            if (draw_data == nullptr || draw_data->CmdListsCount == 0)
            {
                return true;
            }

            GE::RenderCore::UIRenderData& ui = out_frame.ui;
            ui.display_position = {
                draw_data->DisplayPos.x, draw_data->DisplayPos.y};
            ui.display_size = {
                draw_data->DisplaySize.x, draw_data->DisplaySize.y};
            ui.framebuffer_scale = {
                draw_data->FramebufferScale.x, draw_data->FramebufferScale.y};
            ui.vertices.reserve(static_cast<std::size_t>(draw_data->TotalVtxCount));
            ui.indices.reserve(static_cast<std::size_t>(draw_data->TotalIdxCount));

            std::uint32_t global_vertex_offset = 0;
            std::uint32_t global_index_offset = 0;
            for (int list_index = 0;
                list_index < draw_data->CmdListsCount; ++list_index)
            {
                const ImDrawList* list = draw_data->CmdLists[list_index];
                for (const ImDrawVert& vertex : list->VtxBuffer)
                {
                    ui.vertices.push_back({
                        .position = {vertex.pos.x, vertex.pos.y},
                        .texture_coordinate = {vertex.uv.x, vertex.uv.y},
                        .color = vertex.col});
                }
                for (ImDrawIdx index : list->IdxBuffer)
                {
                    ui.indices.push_back(static_cast<std::uint32_t>(index));
                }
                for (const ImDrawCmd& draw : list->CmdBuffer)
                {
                    if (draw.UserCallback != nullptr || draw.ElemCount == 0)
                    {
                        continue;
                    }
                    const std::uintptr_t texture = reinterpret_cast<std::uintptr_t>(
                        draw.GetTexID());
                    ui.commands.push_back({
                        .clip_rectangle = {
                            draw.ClipRect.x, draw.ClipRect.y,
                            draw.ClipRect.z, draw.ClipRect.w},
                        .texture = texture == SceneViewportTextureId
                            ? GE::RenderCore::UITexture::SceneViewport
                            : GE::RenderCore::UITexture::FontAtlas,
                        .index_count = draw.ElemCount,
                        .first_index = global_index_offset + draw.IdxOffset,
                        .vertex_offset = static_cast<std::int32_t>(
                            global_vertex_offset + draw.VtxOffset)});
                }
                global_vertex_offset += static_cast<std::uint32_t>(
                    list->VtxBuffer.Size);
                global_index_offset += static_cast<std::uint32_t>(
                    list->IdxBuffer.Size);
            }
            return true;
        }
        catch (...)
        {
            out_frame.ui = {};
            return false;
        }
    }

    bool EditorUserInterface::ProcessPlatformMessage(
        const GE::Platform::PlatformMessage& message) noexcept
    {
        if (!m_initialized || m_context == nullptr)
        {
            return false;
        }
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_context));
        return ImGui_ImplWin32_WndProcHandler(
            reinterpret_cast<HWND>(message.native_window),
            message.message,
            static_cast<WPARAM>(message.word_parameter),
            static_cast<LPARAM>(message.long_parameter)) != 0;
    }

    void EditorUserInterface::Shutdown() noexcept
    {
        if (m_context == nullptr)
        {
            return;
        }
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_context));
        if (m_initialized)
        {
            ImGui_ImplWin32_Shutdown();
        }
        ImGui::DestroyContext(static_cast<ImGuiContext*>(m_context));
        m_context = nullptr;
        m_initialized = false;
        m_viewport_navigation_active = false;
    }
}
