#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "EditorUserInterface.h"

#include "EditorCommand.h"
#include "EditorSession.h"

#include <Runtime/GameObject.h>
#include <Runtime/World.h>
#include <Runtime/WorldContext.h>
#include <FieldInfo.h>
#include <TypeInfo.h>
#include <TypeRegistry.h>
#include <Core/Serialization/BuiltinSerialization.h>
#include <Assets/Registry/AssetSerialization.h>

#include <Windows.h>
#include <commdlg.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <type_traits>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM word_parameter,
                                                             LPARAM long_parameter);

namespace GE::Editor
{
    namespace
    {
        constexpr std::uintptr_t FontTextureId = 1;
        constexpr std::uintptr_t SceneViewportTextureId = 2;

        template <std::size_t Size> void CopyToBuffer(const std::string& source, std::array<char, Size>& buffer)
        {
            const std::size_t count = (std::min)(source.size(), Size - 1);
            std::memcpy(buffer.data(), source.data(), count);
            buffer[count] = '\0';
        }

        void Queue(EditorSession& editor, EditorCommand command)
        {
            (void)editor.QueueCommand(std::move(command));
        }

        bool EditText(const char* label, std::string& text)
        {
            std::vector<char> buffer(text.size() + 32);
            std::copy(text.begin(), text.end(), buffer.begin());
            const auto resize = +[](ImGuiInputTextCallbackData* data) -> int
            {
                auto& storage = *static_cast<std::vector<char>*>(data->UserData);
                storage.resize(data->BufSize);
                data->Buf = storage.data();
                return 0;
            };
            if (!ImGui::InputText(label, buffer.data(), buffer.size(), ImGuiInputTextFlags_CallbackResize, resize,
                                  &buffer))
                return false;
            text = buffer.data();
            return true;
        }

        template <typename T>
        bool DrawScalar(const char* label, ImGuiDataType kind, GE::Reflection::ConstValueView read,
                        GE::Reflection::ValueView write, const GE::Reflection::EditorFieldInfo& editor)
        {
            T value = *static_cast<const T*>(read.Data());
            const T minimum = static_cast<T>(editor.minimum), maximum = static_cast<T>(editor.maximum);
            const bool bounded = editor.maximum > editor.minimum;
            const float speed = std::is_integral_v<T> && editor.speed == 0.01F ? 1.0F : editor.speed;
            const bool changed = ImGui::DragScalar(label, kind, &value, speed, bounded ? &minimum : nullptr,
                                                   bounded ? &maximum : nullptr);
            if (changed && write.IsValid())
                *static_cast<T*>(write.Data()) = value;
            return changed && write.IsValid();
        }

        bool DrawReflectedValue(const char* label, GE::Reflection::ConstValueView read, GE::Reflection::ValueView write,
                                const GE::Reflection::TypeRegistry& types,
                                const GE::Reflection::EditorFieldInfo& editor = {})
        {
            using namespace GE;
            const auto* type = types.FindType(read.GetTypeId());
            if (!type)
                return false;
            ImGui::BeginDisabled(!write.IsValid());
            bool changed = false;
            if (read.GetTypeId() == Reflection::GetTypeId<bool>())
            {
                bool value = *static_cast<const bool*>(read.Data());
                if (ImGui::Checkbox(label, &value) && write.IsValid())
                {
                    *static_cast<bool*>(write.Data()) = value;
                    changed = true;
                }
            }
#define MENGINE_DRAW_SCALAR(T, Kind)                                                                                   \
    else if (read.GetTypeId() == Reflection::GetTypeId<T>()) changed = DrawScalar<T>(label, Kind, read, write, editor)
            MENGINE_DRAW_SCALAR(char, std::is_signed_v<char> ? ImGuiDataType_S8 : ImGuiDataType_U8);
            MENGINE_DRAW_SCALAR(signed char, ImGuiDataType_S8);
            MENGINE_DRAW_SCALAR(unsigned char, ImGuiDataType_U8);
            MENGINE_DRAW_SCALAR(short, ImGuiDataType_S16);
            MENGINE_DRAW_SCALAR(unsigned short, ImGuiDataType_U16);
            MENGINE_DRAW_SCALAR(int, ImGuiDataType_S32);
            MENGINE_DRAW_SCALAR(unsigned int, ImGuiDataType_U32);
            MENGINE_DRAW_SCALAR(long, sizeof(long) == 4 ? ImGuiDataType_S32 : ImGuiDataType_S64);
            MENGINE_DRAW_SCALAR(unsigned long, sizeof(unsigned long) == 4 ? ImGuiDataType_U32 : ImGuiDataType_U64);
            MENGINE_DRAW_SCALAR(long long, ImGuiDataType_S64);
            MENGINE_DRAW_SCALAR(unsigned long long, ImGuiDataType_U64);
            MENGINE_DRAW_SCALAR(float, ImGuiDataType_Float);
            MENGINE_DRAW_SCALAR(double, ImGuiDataType_Double);
#undef MENGINE_DRAW_SCALAR
            else if (read.GetTypeId() == Reflection::GetTypeId<std::string>())
            {
                std::string value = *static_cast<const std::string*>(read.Data());
                if (EditText(label, value) && write.IsValid())
                {
                    *static_cast<std::string*>(write.Data()) = std::move(value);
                    changed = true;
                }
            }
            else if (read.GetTypeId() == Reflection::GetTypeId<Asset::AssetId>())
            {
                std::string text = Asset::ToString(*static_cast<const Asset::AssetId*>(read.Data()));
                if (EditText(label, text) && write.IsValid())
                    if (auto value = Asset::ParseAssetId(text))
                    {
                        *static_cast<Asset::AssetId*>(write.Data()) = value.Value();
                        changed = true;
                    }
            }
            else if (type->GetTypeKind() == Reflection::TypeKind::Enum)
            {
                const auto* info = type->GetEnumInfo();
                const auto raw = info->read_raw_value(read.Data());
                const char* selected = "<unknown>";
                for (const auto& entry : info->values)
                    if (entry.raw_value == raw)
                        selected = entry.name.c_str();
                if (ImGui::BeginCombo(label, selected))
                {
                    for (const auto& entry : info->values)
                        if (ImGui::Selectable(entry.name.c_str(), entry.raw_value == raw) && write.IsValid())
                            changed |= info->write_raw_value(write.Data(), entry.raw_value);
                    ImGui::EndCombo();
                }
            }
            else if (read.GetTypeId() == Reflection::GetTypeId<Math::Vec2>() ||
                     read.GetTypeId() == Reflection::GetTypeId<Math::Vec3>() ||
                     read.GetTypeId() == Reflection::GetTypeId<Math::Vec4>())
            {
                const int count = read.GetTypeId() == Reflection::GetTypeId<Math::Vec2>()   ? 2
                                  : read.GetTypeId() == Reflection::GetTypeId<Math::Vec3>() ? 3
                                                                                            : 4;
                float values[4]{};
                const float* source = count == 2   ? static_cast<const Math::Vec2*>(read.Data())->data()
                                      : count == 3 ? static_cast<const Math::Vec3*>(read.Data())->data()
                                                   : static_cast<const Math::Vec4*>(read.Data())->data();
                std::copy_n(source, count, values);
                const float minimum = static_cast<float>(editor.minimum), maximum = static_cast<float>(editor.maximum);
                const bool bounded = editor.maximum > editor.minimum;
                if (ImGui::DragScalarN(label, ImGuiDataType_Float, values, count, editor.speed,
                                       bounded ? &minimum : nullptr, bounded ? &maximum : nullptr) &&
                    write.IsValid())
                {
                    float* destination = count == 2   ? static_cast<Math::Vec2*>(write.Data())->data()
                                         : count == 3 ? static_cast<Math::Vec3*>(write.Data())->data()
                                                      : static_cast<Math::Vec4*>(write.Data())->data();
                    std::copy_n(values, count, destination);
                    changed = true;
                }
            }
            else if (!type->GetFields().empty() && ImGui::TreeNode(label))
            {
                for (const auto& field : type->GetFields())
                {
                    const auto& options = field.GetEditorInfo();
                    const std::string child_label(options.label.empty() ? field.GetName() : options.label);
                    ImGui::PushID(&field);
                    changed |= DrawReflectedValue(child_label.c_str(), field.GetValue(read),
                                                  field.IsTransient() ? Reflection::ValueView{} : field.GetValue(write),
                                                  types, options);
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::EndDisabled();
            return changed;
        }

        void ChooseScene(EditorSession& editor, bool save)
        {
            std::array<wchar_t, 32768> filename{};
            const auto current = editor.GetScenePath().wstring();
            std::copy_n(current.data(), (std::min)(current.size(), filename.size() - 1), filename.data());
            OPENFILENAMEW dialog{sizeof(dialog)};
            dialog.hwndOwner = GetActiveWindow();
            dialog.lpstrFilter = L"MEngine Scene (*.mscene)\0*.mscene\0\0";
            dialog.lpstrFile = filename.data();
            dialog.nMaxFile = static_cast<DWORD>(filename.size());
            dialog.lpstrDefExt = L"mscene";
            dialog.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
            if (save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog))
                Queue(editor, {.type = save ? EditorCommandType::SaveScene : EditorCommandType::LoadScene,
                               .path = std::filesystem::path(filename.data())});
        }

        void SaveScene(EditorSession& editor)
        {
            if (editor.GetScenePath().empty())
                ChooseScene(editor, true);
            else
                Queue(editor, {.type = EditorCommandType::SaveScene, .path = editor.GetScenePath()});
        }

        [[nodiscard]] float DistanceToSegmentSquared(ImVec2 point, ImVec2 start, ImVec2 end) noexcept
        {
            const float segment_x = end.x - start.x;
            const float segment_y = end.y - start.y;
            const float length_squared = segment_x * segment_x + segment_y * segment_y;
            if (length_squared <= 0.0F)
            {
                const float x = point.x - start.x;
                const float y = point.y - start.y;
                return x * x + y * y;
            }
            const float projection =
                (std::max)(0.0F, (std::min)(1.0F, ((point.x - start.x) * segment_x + (point.y - start.y) * segment_y) /
                                                      length_squared));
            const float nearest_x = start.x + segment_x * projection;
            const float nearest_y = start.y + segment_y * projection;
            const float x = point.x - nearest_x;
            const float y = point.y - nearest_y;
            return x * x + y * y;
        }

        [[nodiscard]] std::uint32_t GizmoColor(EditorGizmoAxis axis, bool highlighted) noexcept
        {
            if (highlighted)
            {
                return IM_COL32(255, 230, 80, 255);
            }
            switch (axis)
            {
            case EditorGizmoAxis::X:
                return IM_COL32(230, 70, 70, 255);
            case EditorGizmoAxis::Y:
                return IM_COL32(80, 210, 100, 255);
            case EditorGizmoAxis::Z:
                return IM_COL32(70, 130, 240, 255);
            default:
                return IM_COL32(255, 255, 255, 255);
            }
        }

        void BuildEditorDockspace()
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
            ImGui::SetNextWindowViewport(viewport->ID);

            constexpr ImGuiWindowFlags HostWindowFlags =
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoNavFocus;
            constexpr ImGuiDockNodeFlags DockspaceFlags = ImGuiDockNodeFlags_DockSpace;

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
                const ImGuiID inspector =
                    ImGui::DockBuilderSplitNode(main_area, ImGuiDir_Right, 0.25F, nullptr, &main_area);

                ImGuiID upper_area = main_area;
                ImGuiID bottom_area =
                    ImGui::DockBuilderSplitNode(upper_area, ImGuiDir_Down, 0.30F, nullptr, &upper_area);

                ImGuiID viewport_area = upper_area;
                const ImGuiID hierarchy =
                    ImGui::DockBuilderSplitNode(viewport_area, ImGuiDir_Left, 0.30F, nullptr, &viewport_area);

                ImGuiID scene_area = bottom_area;
                const ImGuiID play_area =
                    ImGui::DockBuilderSplitNode(scene_area, ImGuiDir_Right, 0.24F, nullptr, &scene_area);

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
    } // namespace

    EditorUserInterface::~EditorUserInterface() noexcept
    {
        Shutdown();
    }

    bool EditorUserInterface::Initialize(std::uintptr_t native_window, EditorFontAtlas& out_font_atlas,
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
            const std::size_t byte_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
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

    bool EditorUserInterface::BuildFrame(EditorSession& editor, GE::RenderCore::RenderFrame& out_frame) noexcept
    {
        if (!m_initialized || m_context == nullptr)
        {
            return true;
        }
        try
        {
            out_frame.gizmo = {};
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
                        ChooseScene(editor, false);
                    }
                    if (ImGui::MenuItem("Save"))
                    {
                        SaveScene(editor);
                    }
                    if (ImGui::MenuItem("Save As..."))
                        ChooseScene(editor, true);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Edit"))
                {
                    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, editor.CanUndo()))
                    {
                        Queue(editor, {.type = EditorCommandType::Undo});
                    }
                    if (ImGui::MenuItem("Redo", "Ctrl+Y", false, editor.CanRedo()))
                    {
                        Queue(editor, {.type = EditorCommandType::Redo});
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }

            const ImGuiIO& shortcut_io = ImGui::GetIO();
            if (!shortcut_io.WantTextInput && shortcut_io.KeyCtrl)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
                {
                    Queue(editor, {.type = shortcut_io.KeyShift ? EditorCommandType::Redo : EditorCommandType::Undo});
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
                {
                    Queue(editor, {.type = EditorCommandType::Redo});
                }
            }

            BuildEditorDockspace();

            ImGui::Begin("Scene");
            const auto scene_path_label = editor.GetScenePath().u8string();
            ImGui::TextWrapped("%s", scene_path_label.empty()
                                         ? "Untitled scene"
                                         : reinterpret_cast<const char*>(scene_path_label.c_str()));
            if (ImGui::Button("New"))
            {
                Queue(editor, {.type = EditorCommandType::NewScene});
            }
            ImGui::SameLine();
            if (ImGui::Button("Open"))
            {
                ChooseScene(editor, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("Save"))
            {
                SaveScene(editor);
            }
            if (!editor.GetWorkflowStatus().empty())
            {
                ImGui::TextWrapped("%s", editor.GetWorkflowStatus().c_str());
            }
            ImGui::TextUnformatted(editor.IsSceneDirty() ? "Scene modified *" : "Scene saved");
            ImGui::Checkbox("Temporal AA (TAA)", &m_enable_taa);
            out_frame.enable_taa = m_enable_taa;
            ImGui::Checkbox("Diffuse GI (one bounce)", &m_enable_gi);
            out_frame.enable_gi = m_enable_gi;
            ImGui::Checkbox("Show motion vectors", &m_show_motion_vectors);
            out_frame.visualize_motion_vectors = m_show_motion_vectors;
            if (m_show_motion_vectors)
                ImGui::TextUnformatted("Gray: still | Red: horizontal | Green: vertical");
            if (auto* context = editor.GetEditorWorldContext();
                context && context->GetWorld() && ImGui::CollapsingHeader("Lighting"))
            {
                auto& settings = context->GetWorld()->GetSceneConfiguration().rendering;
                ImGui::BeginDisabled(editor.IsPlaySessionRunning());
                bool changed = false;
                changed |= ImGui::Checkbox("Normal debug", &settings.normal_debug);
                changed |= ImGui::DragFloat3("Sun direction", settings.light_direction.data(), 0.01f, -1, 1);
                changed |= ImGui::ColorEdit3("Sun color", settings.light_color.data());
                changed |= ImGui::DragFloat("Sun intensity", &settings.light_intensity, 0.05f, 0, 100);
                changed |= ImGui::SliderFloat("Environment intensity", &settings.environment_intensity, 0, 4);
                changed |= ImGui::SliderFloat("Environment rotation", &settings.environment_rotation, -180, 180);
                changed |= ImGui::SliderFloat("Ambient fill", &settings.ambient, 0, 1);
                changed |= ImGui::SliderFloat("Exposure", &settings.exposure, 0.01f, 8);
                changed |= ImGui::Checkbox("Sun shadows", &settings.shadows);
                changed |= ImGui::SliderFloat("Shadow distance", &settings.shadow_distance, 5, 300);
                for (size_t i = 0; i < settings.lights.size(); ++i)
                {
                    ImGui::PushID(int(i));
                    auto& light = settings.lights[i];
                    if (ImGui::TreeNode("Light", "%s %zu", light.spot ? "Spot" : "Point", i + 1))
                    {
                        changed |= ImGui::Checkbox("Spot light", &light.spot);
                        changed |= ImGui::DragFloat3("Position", light.position.data(), 0.1f);
                        changed |= ImGui::ColorEdit3("Color", light.color.data());
                        changed |= ImGui::DragFloat("Intensity", &light.intensity, 0.2f, 0, 1000);
                        changed |= ImGui::DragFloat("Range", &light.range, 0.1f, 0.1f, 200);
                        if (light.spot)
                        {
                            changed |= ImGui::DragFloat3("Direction", light.direction.data(), 0.01f, -1, 1);
                            changed |=
                                ImGui::SliderFloat("Inner angle", &light.inner_angle, 0, light.outer_angle - 0.1f);
                            changed |=
                                ImGui::SliderFloat("Outer angle", &light.outer_angle, light.inner_angle + 0.1f, 89);
                            Math::Vec3 direction{light.direction[0], light.direction[1], light.direction[2]};
                            if (!Math::Normalize(direction))
                                light.direction = {0, -1, 0};
                        }
                        const bool remove = ImGui::Button("Remove light");
                        ImGui::TreePop();
                        if (remove)
                        {
                            settings.lights.erase(settings.lights.begin() + i);
                            changed = true;
                            ImGui::PopID();
                            break;
                        }
                    }
                    ImGui::PopID();
                }
                if (settings.lights.size() < RenderCore::MaxLocalLights && ImGui::Button("Add point light"))
                {
                    settings.lights.push_back({.position = {0, 3, 0}});
                    changed = true;
                }
                if (changed)
                    editor.NotifyRenderingChanged();
                ImGui::EndDisabled();
            }
            if (out_frame.gpu_timings.valid && ImGui::CollapsingHeader("GPU timings"))
            {
                constexpr const char* names[] = {"Culling", "Sun shadows", "Scene + sky", "TAA + display", "Editor"};
                double total = 0;
                for (size_t i = 0; i < out_frame.gpu_timings.milliseconds.size(); ++i)
                {
                    ImGui::Text("%s: %.3f ms", names[i], out_frame.gpu_timings.milliseconds[i]);
                    total += out_frame.gpu_timings.milliseconds[i];
                }
                ImGui::Text("Total: %.3f ms", total);
            }
            ClusterDebugSettings& cluster_debug = editor.GetClusterDebugSettings();
            if (cluster_debug.cluster_count != 0U)
            {
                ImGui::Separator();
                ImGui::Text("Leaf clusters: %u", cluster_debug.cluster_count);
                ImGui::Checkbox("Show clusters", &cluster_debug.visible);
                if (cluster_debug.visible)
                {
                    constexpr const char* ColorModes[] = {"Cluster ID", "Batch", "Highlight ID"};
                    ImGui::Combo("Color by", &cluster_debug.color_mode, ColorModes, 3);
                    if (cluster_debug.color_mode == 1)
                    {
                        ImGui::DragInt("Clusters per color", &cluster_debug.batch_size, 1.0F, 1, 1024);
                        cluster_debug.batch_size = std::max(cluster_debug.batch_size, 1);
                    }
                    else if (cluster_debug.color_mode == 2)
                    {
                        ImGui::SliderInt("Cluster ID", &cluster_debug.selected_cluster, 0,
                                         static_cast<int>(cluster_debug.cluster_count - 1U));
                    }
                }
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
            ImGui::TextUnformatted(editor.IsPlaySessionRunning() ? "PIE is running (editing locked)" : "Edit mode");
            ImGui::End();

            GE::Runtime::WorldContext* context =
                editor.IsPlaySessionRunning() ? editor.GetPIEWorldContext() : editor.GetEditorWorldContext();
            GE::Runtime::World* world = context == nullptr ? nullptr : context->GetWorld();

            ImGui::Begin("Viewport");
            ImVec2 available = ImGui::GetContentRegionAvail();
            available.x = (std::max)(available.x, 1.0F);
            available.y = (std::max)(available.y, 1.0F);
            out_frame.scene_extent = {.width = static_cast<std::uint32_t>(available.x),
                                      .height = static_cast<std::uint32_t>(available.y)};
            ImGui::Image(reinterpret_cast<ImTextureID>(SceneViewportTextureId), available);

            const bool edit_mode = !editor.IsPlaySessionRunning();
            const bool image_hovered = ImGui::IsItemHovered();
            const ImVec2 image_min = ImGui::GetItemRectMin();
            const ImVec2 image_max = ImGui::GetItemRectMax();
            bool gizmo_consumed_click = false;

            EditorGizmoAxis hovered_axis = EditorGizmoAxis::None;
            std::array<TranslationGizmoAxisProjection, 3> axis_projections{};
            std::array<bool, 3> axis_projection_valid{};
            constexpr std::array<EditorGizmoAxis, 3> GizmoAxes{EditorGizmoAxis::X, EditorGizmoAxis::Y,
                                                               EditorGizmoAxis::Z};
            GE::Runtime::GameObject* viewport_selected =
                edit_mode && world != nullptr ? world->FindGameObjectById(editor.GetSelectedObjectId()) : nullptr;
            if (viewport_selected != nullptr)
            {
                GE::RenderCore::RenderView gizmo_view;
                if (editor.GetEditorCamera().BuildRenderView(out_frame.scene_extent, gizmo_view))
                {
                    const GE::Math::Mat4 view_projection = gizmo_view.view * gizmo_view.projection;
                    for (std::size_t axis_index = 0; axis_index < GizmoAxes.size(); ++axis_index)
                    {
                        const EditorGizmoAxis axis = GizmoAxes[axis_index];
                        TranslationGizmoAxisProjection& projection = axis_projections[axis_index];
                        if (!BuildTranslationGizmoAxisProjection(
                                GE::Math::TransformPoint({}, viewport_selected->GetWorldMatrix()), axis,
                                view_projection, out_frame.scene_extent, projection))
                        {
                            continue;
                        }
                        axis_projection_valid[axis_index] = true;
                    }

                    // 裁剪空间深度和投影比例只生成一份公共世界缩放；三轴不会再被
                    // 分别拉成等长，朝向镜头的轴会保留自然的透视缩短。
                    float gizmo_world_length = 0.0F;
                    bool has_gizmo_world_length = false;
                    for (std::size_t axis_index = 0; axis_index < GizmoAxes.size(); ++axis_index)
                    {
                        if (!axis_projection_valid[axis_index])
                        {
                            continue;
                        }
                        if (ComputeTranslationGizmoWorldLength(axis_projections[axis_index].origin_clip,
                                                               gizmo_view.projection, out_frame.scene_extent, 72.0F,
                                                               gizmo_world_length))
                        {
                            has_gizmo_world_length = true;
                            break;
                        }
                    }

                    std::array<float, 3> axis_screen_lengths{};
                    if (has_gizmo_world_length)
                    {
                        const ImVec2 mouse = ImGui::GetMousePos();
                        float closest_axis_distance_squared = 64.0F;
                        for (std::size_t axis_index = 0; axis_index < GizmoAxes.size(); ++axis_index)
                        {
                            if (!axis_projection_valid[axis_index] ||
                                !ComputeTranslationGizmoScreenLength(axis_projections[axis_index],
                                                                     out_frame.scene_extent, gizmo_world_length,
                                                                     axis_screen_lengths[axis_index]))
                            {
                                axis_projection_valid[axis_index] = false;
                                continue;
                            }
                            const TranslationGizmoAxisProjection& projection = axis_projections[axis_index];
                            const ImVec2 origin{image_min.x + projection.origin_pixels[0],
                                                image_min.y + projection.origin_pixels[1]};
                            const ImVec2 endpoint{
                                origin.x + projection.screen_direction[0] * axis_screen_lengths[axis_index],
                                origin.y + projection.screen_direction[1] * axis_screen_lengths[axis_index]};
                            const float distance_squared = DistanceToSegmentSquared(mouse, origin, endpoint);
                            if (image_hovered && distance_squared < closest_axis_distance_squared)
                            {
                                closest_axis_distance_squared = distance_squared;
                                hovered_axis = GizmoAxes[axis_index];
                            }
                        }

                        for (std::size_t axis_index = 0; axis_index < GizmoAxes.size(); ++axis_index)
                        {
                            if (!axis_projection_valid[axis_index])
                            {
                                continue;
                            }
                            const EditorGizmoAxis axis = GizmoAxes[axis_index];
                            const bool highlighted = axis == hovered_axis || axis == m_gizmo_drag_axis;
                            if (!AppendTranslationGizmoAxisGeometry(
                                    axis_projections[axis_index], out_frame.scene_extent, gizmo_world_length,
                                    highlighted ? 5.0F : 3.0F, highlighted ? 6.0F : 4.0F, GizmoColor(axis, highlighted),
                                    out_frame.gizmo))
                            {
                                return false;
                            }
                        }
                    }
                }
            }

            if (m_gizmo_drag_axis != EditorGizmoAxis::None)
            {
                GE::Runtime::GameObject* target = world == nullptr ? nullptr : world->FindGameObject(m_gizmo_target);
                if (!edit_mode || target == nullptr || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                {
                    Queue(editor, {.type = EditorCommandType::CancelTransformEdit});
                    m_gizmo_drag_axis = EditorGizmoAxis::None;
                    m_gizmo_target = {};
                }
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                {
                    const ImVec2 mouse = ImGui::GetMousePos();
                    EditorCommand preview{.type = EditorCommandType::PreviewTransformEdit, .target = m_gizmo_target};
                    preview.transform = ApplyTranslationGizmoDrag(
                        m_gizmo_initial_transform, m_gizmo_drag_axis, m_gizmo_drag_projection,
                        {mouse.x - m_gizmo_drag_start_mouse[0], mouse.y - m_gizmo_drag_start_mouse[1]});
                    if (const auto* parent = target->GetParent())
                    {
                        GE::Math::Mat4 inverse;
                        if (GE::Math::TryInverseAffine(parent->GetWorldMatrix(), inverse))
                        {
                            const auto world_delta =
                                preview.transform.translation - m_gizmo_initial_transform.translation;
                            preview.transform.translation =
                                m_gizmo_initial_transform.translation + GE::Math::TransformVector(world_delta, inverse);
                        }
                        else
                            preview.transform = m_gizmo_initial_transform;
                    }
                    Queue(editor, std::move(preview));
                    gizmo_consumed_click = true;
                }
                else
                {
                    Queue(editor, {.type = EditorCommandType::CommitTransformEdit});
                    m_gizmo_drag_axis = EditorGizmoAxis::None;
                    m_gizmo_target = {};
                    gizmo_consumed_click = true;
                }
            }
            else if (viewport_selected != nullptr && hovered_axis != EditorGizmoAxis::None &&
                     ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                const std::size_t axis_index = hovered_axis == EditorGizmoAxis::X   ? 0
                                               : hovered_axis == EditorGizmoAxis::Y ? 1
                                                                                    : 2;
                const ImVec2 mouse = ImGui::GetMousePos();
                m_gizmo_drag_axis = hovered_axis;
                m_gizmo_target = viewport_selected->GetHandle();
                m_gizmo_initial_transform = viewport_selected->GetTransform();
                m_gizmo_drag_projection = axis_projections[axis_index];
                m_gizmo_drag_start_mouse = {mouse.x, mouse.y};
                Queue(editor, {.type = EditorCommandType::BeginTransformEdit, .target = m_gizmo_target});
                gizmo_consumed_click = true;
            }

            if (!edit_mode)
            {
                m_viewport_navigation_active = false;
            }
            else
            {
                if (image_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
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
                    EditorCameraInput input{.forward_axis = (ImGui::IsKeyDown(ImGuiKey_W) ? 1.0F : 0.0F) -
                                                            (ImGui::IsKeyDown(ImGuiKey_S) ? 1.0F : 0.0F),
                                            .right_axis = (ImGui::IsKeyDown(ImGuiKey_D) ? 1.0F : 0.0F) -
                                                          (ImGui::IsKeyDown(ImGuiKey_A) ? 1.0F : 0.0F),
                                            .up_axis = (ImGui::IsKeyDown(ImGuiKey_Q) ? 1.0F : 0.0F) -
                                                       (ImGui::IsKeyDown(ImGuiKey_E) ? 1.0F : 0.0F),
                                            .mouse_delta_x = io.MouseDelta.x,
                                            .mouse_delta_y = io.MouseDelta.y,
                                            .wheel_delta = io.MouseWheel};
                    editor.UpdateEditorCamera(input, io.DeltaTime);
                    ImGui::SetMouseCursor(ImGuiMouseCursor_None);
                }

                if (!m_viewport_navigation_active && !gizmo_consumed_click &&
                    ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    const ImVec2 mouse = ImGui::GetMousePos();
                    const float width = image_max.x - image_min.x;
                    const float height = image_max.y - image_min.y;
                    if (width > 0.0F && height > 0.0F)
                    {
                        editor.PickEditorObject({(mouse.x - image_min.x) / width, (mouse.y - image_min.y) / height},
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
                const auto drawTree = [&](auto&& self, const GE::Runtime::GameObject& object) -> void
                {
                    const auto handle = object.GetHandle();
                    ImGui::PushID(int(handle.index));
                    ImGui::PushID(int(handle.generation));
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                    if (object.GetChildren().empty())
                        flags |= ImGuiTreeNodeFlags_Leaf;
                    if (object.GetId() == editor.GetSelectedObjectId())
                        flags |= ImGuiTreeNodeFlags_Selected;
                    const bool open = ImGui::TreeNodeEx(object.GetName().c_str(), flags);
                    if (ImGui::IsItemClicked())
                        (void)editor.SelectObject(object.GetId());
                    if (open)
                    {
                        for (auto child : object.GetChildren())
                            self(self, *world->FindGameObject(child));
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    ImGui::PopID();
                };
                for (auto handle : world->GetGameObjectHandles())
                {
                    const auto* object = world->FindGameObject(handle);
                    if (!object->GetParent())
                        drawTree(drawTree, *object);
                }
            }
            ImGui::End();

            ImGui::Begin("Inspector");
            GE::Runtime::GameObject* selected =
                world == nullptr ? nullptr : world->FindGameObjectById(editor.GetSelectedObjectId());
            if (selected == nullptr)
            {
                editor.ClearSelection();
                ImGui::TextUnformatted("Select an object in the Hierarchy.");
            }
            else
            {
                ImGui::Text("ID: %llu", static_cast<unsigned long long>(selected->GetId().value));
                std::array<char, 256> name{};
                CopyToBuffer(selected->GetName(), name);
                if (!editor.IsPlaySessionRunning() && ImGui::InputText("Name", name.data(), name.size()))
                {
                    EditorCommand command{.type = EditorCommandType::RenameObject};
                    command.target = selected->GetHandle();
                    command.name = name.data();
                    Queue(editor, std::move(command));
                }

                GE::Runtime::TransformComponent transform = selected->GetTransform();
                const bool transform_editable = !editor.IsPlaySessionRunning();
                auto DrawTransformField =
                    [&](const char* label, float* values, float speed, float minimum = 0.0F, float maximum = 0.0F)
                {
                    const bool changed = ImGui::DragFloat3(label, values, speed, minimum, maximum);
                    if (!transform_editable)
                    {
                        return;
                    }
                    if (ImGui::IsItemActivated())
                    {
                        Queue(editor, {.type = EditorCommandType::BeginTransformEdit, .target = selected->GetHandle()});
                    }
                    if (changed)
                    {
                        EditorCommand preview{.type = EditorCommandType::PreviewTransformEdit,
                                              .target = selected->GetHandle()};
                        preview.transform = transform;
                        Queue(editor, std::move(preview));
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        Queue(editor, {.type = EditorCommandType::CommitTransformEdit});
                    }
                };
                DrawTransformField("Translation", transform.translation.data(), 0.05F);
                DrawTransformField("Rotation", transform.rotation_degrees.data(), 0.25F);
                DrawTransformField("Scale", transform.scale.data(), 0.02F, 0.001F, 1000.0F);

                if (GE::Runtime::MeshRendererComponent* mesh = selected->GetMeshRenderer(); mesh != nullptr)
                {
                    ImGui::Separator();
                    ImGui::TextUnformatted("Mesh Renderer");
                    int mesh_id = static_cast<int>(mesh->mesh.value);
                    int material_id = static_cast<int>(mesh->material.value);
                    bool visible = mesh->visible;
                    const bool changed = ImGui::InputInt("Mesh", &mesh_id) | ImGui::InputInt("Material", &material_id) |
                                         ImGui::Checkbox("Visible", &visible);
                    if (!editor.IsPlaySessionRunning() && changed && mesh_id > 0 && material_id > 0)
                    {
                        EditorCommand command{.type = EditorCommandType::PatchComponent};
                        command.target = selected->GetHandle();
                        GE::Runtime::MeshRendererComponent value = *mesh;
                        value.mesh.value = static_cast<std::uint32_t>(mesh_id);
                        value.material.value = static_cast<std::uint32_t>(material_id);
                        value.visible = visible;
                        std::string error;
                        if (SetComponentPayload(command, value, world->GetComponentRegistry(),
                                                editor.GetSerializationContext(), error))
                            Queue(editor, std::move(command));
                    }
                    if (!editor.IsPlaySessionRunning() && ImGui::Button("Remove Mesh Renderer"))
                    {
                        Queue(editor, {.type = EditorCommandType::RemoveComponent,
                                       .target = selected->GetHandle(),
                                       .component_type_id = GE::Runtime::MeshRendererComponent::StaticTypeId});
                    }
                }
                else if (!editor.IsPlaySessionRunning() && ImGui::Button("Add Mesh Renderer"))
                {
                    EditorCommand command{.type = EditorCommandType::AddComponent};
                    command.target = selected->GetHandle();
                    GE::Runtime::MeshRendererComponent value;
                    value.mesh.value = 1;
                    value.material.value = 1;
                    std::string error;
                    if (SetComponentPayload(command, value, world->GetComponentRegistry(),
                                            editor.GetSerializationContext(), error))
                        Queue(editor, std::move(command));
                }

                if (GE::Runtime::CameraComponent* camera = selected->GetCamera(); camera != nullptr)
                {
                    ImGui::Separator();
                    ImGui::TextUnformatted("Camera");
                    GE::Runtime::CameraComponent value = *camera;
                    const bool changed =
                        ImGui::DragFloat("Vertical FOV", &value.vertical_field_of_view_degrees, 0.25F, 1.0F, 179.0F) |
                        ImGui::DragFloat("Near", &value.near_plane, 0.01F, 0.001F, 1000.0F) |
                        ImGui::DragFloat("Far", &value.far_plane, 1.0F, 0.01F, 100000.0F) |
                        ImGui::Checkbox("Primary", &value.primary);
                    if (!editor.IsPlaySessionRunning() && changed)
                    {
                        value.vertical_field_of_view_degrees =
                            std::clamp(value.vertical_field_of_view_degrees, 1.0F, 179.0F);
                        value.near_plane = std::max(value.near_plane, 0.001F);
                        value.far_plane = std::max(value.far_plane, value.near_plane + 0.001F);
                        EditorCommand command{.type = EditorCommandType::PatchComponent};
                        command.target = selected->GetHandle();
                        std::string error;
                        if (SetComponentPayload(command, value, world->GetComponentRegistry(),
                                                editor.GetSerializationContext(), error))
                            Queue(editor, std::move(command));
                    }
                    if (!editor.IsPlaySessionRunning() && ImGui::Button("Remove Camera"))
                    {
                        Queue(editor, {.type = EditorCommandType::RemoveComponent,
                                       .target = selected->GetHandle(),
                                       .component_type_id = GE::Runtime::CameraComponent::StaticTypeId});
                    }
                }
                else if (!editor.IsPlaySessionRunning() && ImGui::Button("Add Camera"))
                {
                    EditorCommand command{.type = EditorCommandType::AddComponent};
                    command.target = selected->GetHandle();
                    GE::Runtime::CameraComponent value;
                    std::string error;
                    if (SetComponentPayload(command, value, world->GetComponentRegistry(),
                                            editor.GetSerializationContext(), error))
                        Queue(editor, std::move(command));
                }

                for (GE::Runtime::Component* component : selected->EnumerateComponents())
                {
                    if (component->GetTypeId() == GE::Runtime::TransformComponent::StaticTypeId ||
                        component->GetTypeId() == GE::Runtime::MeshRendererComponent::StaticTypeId ||
                        component->GetTypeId() == GE::Runtime::CameraComponent::StaticTypeId)
                    {
                        continue;
                    }

                    const GE::Reflection::TypeInfo* type_info =
                        editor.GetReflectionRegistry().FindType(component->GetTypeId());
                    std::unique_ptr<GE::Runtime::Component> draft =
                        world->GetComponentRegistry().CloneConfiguration(*component);
                    if (type_info == nullptr || draft == nullptr)
                        continue;

                    ImGui::PushID(component);
                    ImGui::Separator();
                    ImGui::TextUnformatted(type_info->GetStableName().data());
                    bool changed = false;
                    const GE::Reflection::ValueView owner{draft.get(), draft->GetTypeId()};
                    ImGui::BeginDisabled(editor.IsPlaySessionRunning());
                    for (const GE::Reflection::FieldInfo& field : type_info->GetFields())
                    {
                        const auto& options = field.GetEditorInfo();
                        const std::string label(options.label.empty() ? field.GetName() : options.label);
                        ImGui::PushID(&field);
                        changed |= DrawReflectedValue(label.c_str(), field.GetValue(owner.AsConst()),
                                                      field.IsTransient() ? GE::Reflection::ValueView{}
                                                                          : field.GetValue(owner),
                                                      editor.GetReflectionRegistry(), options);
                        ImGui::PopID();
                    }
                    ImGui::EndDisabled();
                    if (!editor.IsPlaySessionRunning() && changed)
                    {
                        EditorCommand command{.type = EditorCommandType::PatchComponent,
                                              .target = selected->GetHandle()};
                        std::string error;
                        if (SetComponentPayload(command, *draft, world->GetComponentRegistry(),
                                                editor.GetSerializationContext(), error))
                            Queue(editor, std::move(command));
                    }
                    if (!editor.IsPlaySessionRunning() && ImGui::Button("Remove Component"))
                    {
                        Queue(editor, {.type = EditorCommandType::RemoveComponent,
                                       .target = selected->GetHandle(),
                                       .component_type_id = component->GetTypeId()});
                    }
                    ImGui::PopID();
                }

                if (!editor.IsPlaySessionRunning())
                {
                    for (const GE::Runtime::ComponentDescriptor& descriptor :
                         world->GetComponentRegistry().GetDescriptors())
                    {
                        if (descriptor.required || selected->GetComponent(descriptor.type_id) != nullptr ||
                            descriptor.type_id == GE::Runtime::MeshRendererComponent::StaticTypeId ||
                            descriptor.type_id == GE::Runtime::CameraComponent::StaticTypeId)
                        {
                            continue;
                        }
                        const std::string label = "Add " + descriptor.stable_name;
                        if (ImGui::Button(label.c_str()))
                        {
                            std::unique_ptr<GE::Runtime::Component> value = descriptor.factory();
                            EditorCommand command{.type = EditorCommandType::AddComponent,
                                                  .target = selected->GetHandle()};
                            std::string error;
                            if (value != nullptr && SetComponentPayload(command, *value, world->GetComponentRegistry(),
                                                                        editor.GetSerializationContext(), error))
                                Queue(editor, std::move(command));
                        }
                    }
                }

                if (!editor.IsPlaySessionRunning())
                {
                    ImGui::Separator();
                    if (ImGui::Button("Delete Object"))
                    {
                        Queue(editor, {.type = EditorCommandType::DeleteObject, .target = selected->GetHandle()});
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
            ui.display_position = {draw_data->DisplayPos.x, draw_data->DisplayPos.y};
            ui.display_size = {draw_data->DisplaySize.x, draw_data->DisplaySize.y};
            ui.framebuffer_scale = {draw_data->FramebufferScale.x, draw_data->FramebufferScale.y};
            ui.vertices.reserve(static_cast<std::size_t>(draw_data->TotalVtxCount));
            ui.indices.reserve(static_cast<std::size_t>(draw_data->TotalIdxCount));

            std::uint32_t global_vertex_offset = 0;
            std::uint32_t global_index_offset = 0;
            for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index)
            {
                const ImDrawList* list = draw_data->CmdLists[list_index];
                for (const ImDrawVert& vertex : list->VtxBuffer)
                {
                    ui.vertices.push_back({.position = {vertex.pos.x, vertex.pos.y},
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
                    const std::uintptr_t texture = reinterpret_cast<std::uintptr_t>(draw.GetTexID());
                    ui.commands.push_back(
                        {.clip_rectangle = {draw.ClipRect.x, draw.ClipRect.y, draw.ClipRect.z, draw.ClipRect.w},
                         .texture = texture == SceneViewportTextureId ? GE::RenderCore::UITexture::SceneViewport
                                                                      : GE::RenderCore::UITexture::FontAtlas,
                         .index_count = draw.ElemCount,
                         .first_index = global_index_offset + draw.IdxOffset,
                         .vertex_offset = static_cast<std::int32_t>(global_vertex_offset + draw.VtxOffset)});
                }
                global_vertex_offset += static_cast<std::uint32_t>(list->VtxBuffer.Size);
                global_index_offset += static_cast<std::uint32_t>(list->IdxBuffer.Size);
            }
            return true;
        }
        catch (...)
        {
            out_frame.gizmo = {};
            out_frame.ui = {};
            return false;
        }
    }

    bool EditorUserInterface::ProcessPlatformMessage(const GE::Platform::PlatformMessage& message) noexcept
    {
        if (!m_initialized || m_context == nullptr)
        {
            return false;
        }
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_context));
        return ImGui_ImplWin32_WndProcHandler(reinterpret_cast<HWND>(message.native_window), message.message,
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
} // namespace GE::Editor
