# MEngine

用于学习现代 C++、实时渲染和编辑器架构的小型引擎。目前只构建编辑器，支持编辑器内 Play/PIE。

- `Engine::Get()` 管理主循环、窗口、设备、编辑器会话和服务。
- `World → GameObject → Component`，支持父子层级、局部/世界变换、稳定 ID 与句柄。
- `RenderFrame → Renderer → Pass → RHI → D3D12`，帧录制、提交和呈现由 Engine 驱动。
- 场景由 `.mscene` v4 和 AssetId 资源引用表决定；项目文件指定启动场景，编辑器 Open 可选择文件切换。默认展示 RendererLab 骷髅头场景的静态版本，包含立柱、球体、地面、士兵、树木等 50 个对象。
- 保留反射编辑、场景保存、Transform 撤销重做、资产/Cook、GPU 视锥剔除及 Cluster 调试。
- HLSL 在构建时编译成 `.cso`，运行时读取字节码。
- TAA 默认开启，可在 Scene 面板通过 `Temporal AA (TAA)` 切换；历史不包含 Gizmo 和 UI。勾选 `Show motion vectors` 可查看速度，灰色表示静止，红/绿通道分别表示水平/垂直位移，此时暂停 TAA 抖动。

打开前先执行 `powershell -ExecutionPolicy Bypass -File .\GenerateSolution.ps1`，再用 Visual Studio 2026 打开 `MEngine.slnx`。

[构建与验证](BUILDING.md) · [当前架构与调用入口](ARCHITECTURE.md) · [场景文件与资源格式](docs/SceneFiles.md)

源码位于 `Source/`；资源、Shader、第三方依赖、工具和测试分别位于根目录的 `Assets/`、`Shaders/`、`ThirdParty/`、`Tools/`、`Tests/`。`docs/` 保留当前功能说明、架构决策、Nanite 计划和设计参考。

构建产物统一放在 `out/`，根目录 `MEngine.slnx` 由 `GenerateSolution.ps1` 生成，清理时保留。清理构建目录后，重新执行脚本即可更新解决方案及其引用的项目文件。`Source/Runtime/Generated/` 和测试中的生成代码是默认构建所需的源码，需要保留。
