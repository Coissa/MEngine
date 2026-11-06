# 当前架构

## 主循环与所有权

`Source/Engine.cpp` 中的 `Engine::Run()` 是编辑器主循环。`Engine::Get()` 拥有窗口、注册表、Device、Swapchain、Renderer 和 EditorSession。EditorSession 拥有编辑器世界及可选 PIE 副本；PIE 由 WorldContext 和 FixedStepScheduler 驱动，不再经过 GameEngine/GameInstance。

```text
处理窗口消息 → EditorSession::Tick → BuildRenderFrame
  → 处理窗口/Viewport Resize（必要时 WaitIdle）
  → Context::BeginFrame
  → Renderer::Render(cmd, slotIndex, frame, output)
  → Context::Submit → Renderer::CommitFrame → Swapchain::Present
```

Renderer 借用 Device，拥有 PipelineLibrary、SceneAssets、SizeResources 和每帧槽的动态资源。Pass 只接受显式输入与公共 CommandList，不读取 World，不执行 Submit、WaitIdle 或 Present。

## 渲染与同步

固定顺序是 CullPass（可选）、ScenePass::DrawShadows（可选四级 CSM）、ScenePass 的网格与天空线性 HDR 输出、ResolveTAA（可选）、PostProcess::ToneMapScene、EditorDebugPass、UIPass。场景与天空共同输出 RGBA16F 颜色和 RG16F 速度；HDR TAA 后统一做曝光、色调映射与 sRGB 显示编码。速度可视化使用 MotionVectorPass，并暂停 TAA 和抖动。EditorDebugPass 独立绘制 Gizmo，显式绑定显示颜色和场景深度，不写入速度或历史。窗口零尺寸不提交 GPU 工作；Viewport 不可见时编辑器 UI 继续工作。

运动矢量定义为 previousUV - currentUV，使用未抖动矩阵与稳定物体 ID 关联前后表面位置；天空只保留相机旋转。实际光栅化使用 8 帧 Halton(2,3) 抖动，剔除视锥保留半像素边界。Renderer 在 Render 中准备待提交历史，Engine 成功 Submit 后调用 CommitFrame 才推进矩阵、历史纹理索引与抖动序号。Resize、视图失效、相机/投影变化及场景/PIE/Cluster 调试版本变化会重置历史；TAA 开关和速度视图切换也会重新开始累积。历史独立于命令槽。

PostProcess.h/.cpp 提供普通函数，不引入 Pass 继承体系。ResolveTAA 直接读取线性 HDR 场景颜色，以 3×3 Mitchell 滤波重建固定像素网格，使用深度邻域选择前景速度，并做历史重投影、YCoCg 邻域/方差裁剪与运动自适应融合。两张 RGBA16F 历史纹理交替保存纯场景 HDR RGB 和累积样本数；ToneMapScene 输出显示颜色，编辑器复用原场景颜色目标。当前未保存上一帧深度，遮挡显露主要依靠邻域裁剪，快速运动和细小几何仍可能有模糊或残影。

Device 拥有主 GraphicsQueue、Fence、描述符分配器和 CommandContext。三个命令槽与两个交换链缓冲独立；Renderer 只使用 BeginFrame 返回的槽位，不自行推进帧索引。

FrameRecording 仅可移动，离开作用域而未提交时自动 Abort。D3D12 命令列表暂存本次录制的资源状态，提交并建立完成标记后 Commit，取消则 Discard。默认堆 Buffer 按提交边界衰减至 COMMON；Texture 使用显式全资源或指定子资源转换，状态按 Mip/Array 分开追踪，取消录制时整体丢弃暂存状态；SRV/RTV/DSV/UAV 可指定 Mip 和数组切片，支持 Cube 与 Mip/Array 上传。资源状态确认与 GPU 执行完成是两件事，释放资源仍需 Fence 完成。

资源创建返回 unique_ptr；View 拥有描述符并借用 Texture，资源组通过成员析构顺序先释放 View。底层图形失败抛出异常，在编辑器入口统一报告；外部资产、场景和文件解析保留错误返回。RHI 内部契约仅保留必要 Debug 断言。

GPU 时间查询与动态常量一样按帧槽持有。Render 写入分阶段时间标记，CommitFrame 标记已提交，下一次复用该槽时读取已完成的结果；Engine 将纯值计时数据传给编辑器，不增加每帧同步等待。

## 世界与编辑器

World 拥有 GameObject，GameObject 拥有组件。父子关系存代数句柄；SetParent 保持局部 TRS，拒绝跨 World 和环。行向量约定为 `world = local * parentWorld`，删除父对象同时删除子树。World 平铺调度组件，每个对象每次更新一次。

相机、渲染提取、拾取和 Gizmo 消费世界变换；世界轴 Gizmo 的位移转换回父空间后更新局部位置。Hierarchy 展示树，当前没有拖放换父级。

场景写为 v5，使用稳定对象 ID 记录父关系，并保存方向光/曝光、环境照明、阴影、局部灯及场景内编号到 AssetId 的网格、材质引用表；v1–v4 继续可读。PIE 复制对象身份、组件配置和层级，运行变化不写回编辑世界。Transform 撤销重做和现有编辑命令机制继续保留。

组件在 Component 上声明纯虚 Save/Load 接口，由具体组件实现。当前内置组件通过通用 Serializer 遍历反射字段，ComponentRegistry 只保留工厂与版本等注册信息，配置复制通过组件接口执行；序列化上下文由调用方显式传入。场景与编辑命令共用组件接口；内置组件新写入 payload v2，旧版位置字段 payload v1 仍可读取。World 文件中的普通函数负责世界到 RenderFrame 的提取；专用 Inspector 的选择直接保留在编辑器属性面板中。

类型元数据、组件工厂注册、Save/Load 与配置复制由实际组件声明生成。默认构建使用生成文件；启用代码生成后按 Runtime manifest 自动更新。配置复制只处理声明的配置字段，加载保留运行状态；通用 Inspector 支持基础数值、字符串、枚举、向量与嵌套结构。扩展方式与兼容规则见 [反射说明](docs/Reflection.md)。

## 源码组织

`Source/Core` 包含数学、反射、序列化和文件基础设施；`Runtime` 包含世界与组件；`Assets` 包含资产注册、格式、Cook 和 CPU 资源；`Renderer` 包含渲染数据、资源、管线、Pass、RHI/D3D12 与 Nanite；`Editor` 与 `Platform` 各自负责编辑器业务和系统窗口。

渲染硬件以 RTX 5070 为目标，默认选择高性能 D3D12 适配器；启动时查询一次设备能力。Shader 在构建时由 DXC 生成 SM6.6/DXIL；ShaderLibrary 仅加载字节码，RHI 不拥有单独 Shader 对象。材质绑定使用公共绑定编号和长期描述符，不再逐物体逐帧创建 ResourceSet。

材质统一使用 MaterialAsset，源文件与 Cook 共用 PBR 参数；GGX 金属度/粗糙度、法线、自发光、AO、纹理重复与 Alpha Cutout 已接入，颜色纹理通过 sRGB SRV 解码，参数纹理保持线性。见 [材质与色彩空间](docs/Materials.md)。方向光、点光、聚光和环境 IBL 共用固定前向 PBR Shader。环境 Compute 在加载时预计算漫反射 Cube、粗糙度镜面 Cube 和 BRDF LUT；SceneAssets 持有结果。阴影复用 ScenePass 与每物体材质常量，不新增 Pass 继承层或资源管理系统。光源为场景配置中的世界空间值，随 RenderFrame 复制，当前最多 64 盏局部灯；点光/聚光阴影与灯光分块留待后续。场景 Pass 在输出深度/速度前执行透明裁剪，树木和围栏不需要专用几何 Shader。RendererLab 的 DDS 和 M3D 在资产制作时分别导出为 PNG 和烘焙姿态的 glTF，运行时复用通用场景加载链路，不依赖原项目。


## 场景文件与资源加载

`MEngine.project` 的 startup_scene 决定默认场景，--scene 可覆盖；EditorSession 初始化与 Open 通过相同的 SceneSerializer 和资源加载回调准备场景，New 创建空 World。SceneAssetLoader 按场景资源表读取注册资产，复用现有 glTF、Texture 导入器和 Cook ResourceManager；材质引用纹理 AssetId。Engine 等待 GPU 空闲后替换 Renderer 和拾取资源，失败时保留原场景。场景配置随保存、加载及 PIE 复制，不再由 BuiltinScene 或启动代码生成几何。Rotator 是可保存、可编辑的组件，只在 gameplay Tick 中更新旋转。格式见 [场景文件与资源](docs/SceneFiles.md)。
