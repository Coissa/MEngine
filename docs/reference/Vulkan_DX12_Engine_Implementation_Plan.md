# Vulkan + DirectX 12 游戏引擎实现方案

> 目标：从零实现一个结构清晰、可调试、可扩展的 Windows 小型游戏引擎，并通过同一套上层渲染代码支持 Vulkan 与 DirectX 12。

## 1. 文档定位

这份文档回答：

1. 引擎模块应该按照什么顺序实现？
2. 模块之间如何划分边界？
3. Vulkan 与 DX12 如何共享同一套上层 Renderer？
4. 每个阶段做到什么程度才允许进入下一阶段？

Piccolo 源码的阅读顺序见 [Piccolo 源码学习路线](./Piccolo_Source_Learning_Roadmap.md)。

---

## 2. 第一版产品范围

### 2.1 必须支持

- Windows 10/11。
- Vulkan 与 DirectX 12 双后端。
- 启动参数或设置菜单切换图形 API。
- 同一套 HLSL Shader 源码。
- glTF/glb 静态模型。
- PNG、JPG、HDR 纹理。
- Forward PBR。
- 方向光和阴影。
- HDR 与 Tone Mapping。
- 基础场景和组件。
- 场景保存与加载。
- ImGui 编辑器。
- Game View、Hierarchy、Inspector、Asset Browser、Console。
- Vulkan Validation Layer、D3D12 Debug Layer。
- RenderDoc 和 PIX 抓帧。

### 2.2 第一版不做

- 多平台编辑器。
- 大世界和流式加载。
- 网络同步。
- 光线追踪。
- Mesh Shader。
- 完整动画状态机。
- 可视化 Shader 编辑器。
- 多 GPU。
- 自定义脚本语言。
- 高级任务系统。

这些功能应在双后端核心链路稳定后再评估。

---

## 3. 核心设计原则

### 3.1 先纵向打通，再横向扩展

第一条完整链路：

```text
窗口
→ Vulkan 清屏
→ Vulkan 三角形
→ 最小 RHI
→ DX12 三角形
→ Buffer 和 Texture
→ glTF 模型
→ Scene
→ Editor
```

不要先花数月完成日志、反射、ECS、内存分配器和 Job System。

### 3.2 RHI 不能泄漏后端类型

公共头文件中禁止出现：

```text
VkBuffer
VkImage
VkFormat
VkCommandBuffer
ID3D12Resource
ID3D12GraphicsCommandList
DXGI_FORMAT
```

上层只能使用引擎定义的 Handle、Desc、Enum 和 CommandList。

### 3.3 Renderer 不依赖 ECS

```text
Scene / ECS
→ 提取 RenderWorld
→ Renderer 消费 RenderWorld
→ Renderer 调用 RHI
```

Renderer 不应该直接遍历 `MeshRendererComponent`。

### 3.4 每个阶段都必须双后端验证

当最小 RHI 和 DX12 后端建立后，每增加一种 GPU 能力，应尽快在两个后端验证。两个后端的功能差距不应超过两个里程碑。

### 3.5 使用成熟依赖

第一版推荐：

- GLFW：窗口和输入。
- glm：数学。
- spdlog：日志。
- stb_image：基础图片加载。
- fastgltf 或 tinygltf：glTF。
- Dear ImGui：编辑器 UI。
- ImGuizmo：Transform Gizmo。
- DXC：HLSL 编译为 DXIL 和 SPIR-V。
- VMA：Vulkan 内存分配。
- D3D12MA：DX12 内存分配。
- JSON 库：场景和资产描述。

学习重点是引擎架构和显式图形 API，不是重复实现通用库。

---

## 4. 总体架构

```text
Editor
├─ Hierarchy / Inspector / Asset Browser
└─ Editor Camera / Gizmo
                 │
                 ▼
Runtime
├─ Engine Loop
├─ Scene / ECS
├─ Input
└─ Game Systems
                 │ 提取 RenderWorld
                 ▼
Renderer
├─ RenderWorld
├─ Mesh / Material
├─ Forward Pass
├─ Shadow Pass
├─ Post Process
└─ Render Graph（后期）
                 │
                 ▼
RHI
├─ Device
├─ Swapchain
├─ CommandList
├─ Resource
├─ Pipeline
├─ Binding
└─ Synchronization
           │              │
           ▼              ▼
      Vulkan Backend   D3D12 Backend
```

---

## 5. 推荐目录

```text
MyEngine/
├─ CMakeLists.txt
├─ cmake/
├─ Engine/
│  ├─ Core/
│  │  ├─ Base/
│  │  ├─ Log/
│  │  ├─ Math/
│  │  └─ Time/
│  ├─ Platform/
│  │  ├─ Window/
│  │  ├─ Input/
│  │  └─ File/
│  ├─ RHI/
│  │  ├─ Common/
│  │  ├─ Vulkan/
│  │  └─ D3D12/
│  ├─ Renderer/
│  │  ├─ Resources/
│  │  ├─ Passes/
│  │  ├─ Shader/
│  │  └─ RenderGraph/
│  ├─ Asset/
│  │  ├─ Importer/
│  │  └─ Database/
│  ├─ Scene/
│  │  ├─ Components/
│  │  └─ Serialization/
│  └─ Runtime/
├─ Editor/
├─ Shaders/
├─ Assets/
├─ Tests/
└─ Samples/
```

---

## 6. 模块实现顺序

## 6.1 M0：工程骨架

**建议时间：2～3 天**

### 实现内容

- CMake 工程。
- `Engine` 静态库。
- `Editor` 可执行程序。
- Debug、Release 构建配置。
- 第三方依赖管理。
- 统一编译选项。

### 最小目录

```text
Engine/Core/
Editor/main.cpp
CMakeLists.txt
```

### 验收

- Visual Studio 可以生成和编译项目。
- Debug 和 Release 均能运行。
- 编辑器程序调用 Engine 库并输出版本号。

---

## 6.2 M1：Core 基础层

**建议时间：3～5 天**

### 实现内容

- 基础整数类型。
- 日志。
- 断言。
- Timer。
- UUID。
- 不可复制基类。
- 通用 Handle。

### Handle 建议

```cpp
template<typename Tag>
struct Handle
{
    uint32_t index = InvalidIndex;
    uint32_t generation = 0;

    bool isValid() const;
};
```

`generation` 用于检测资源槽位被回收后留下的失效句柄。

### 暂不实现

- 自定义容器。
- 自定义智能指针。
- 通用反射。
- 内存分配器。
- Job System。

### 验收

- 日志可输出时间、级别和源文件。
- Assert 在 Debug 构建中生效。
- Handle Pool 能识别失效句柄。

---

## 6.3 M2：Platform 窗口、输入和文件

**建议时间：3～5 天**

### 接口

```cpp
struct WindowDesc
{
    uint32_t width = 1600;
    uint32_t height = 900;
    std::string title = "MyEngine";
};

class Window
{
public:
    bool initialize(const WindowDesc&);
    void pollEvents();
    bool shouldClose() const;

    uint32_t width() const;
    uint32_t height() const;
    void* nativeHandle() const;
};
```

### 实现内容

- GLFW 窗口。
- Win32 原生 Handle 获取。
- Resize、最小化和恢复。
- 键盘、鼠标和滚轮状态。
- 文本/二进制文件读取。
- 路径规范化。

### 验收

- 空窗口稳定运行。
- Resize 和最小化不崩溃。
- Escape 关闭窗口。
- 输入状态每帧正确更新。

---

## 6.4 M3：原生 Vulkan 最小渲染器

**建议时间：2～4 周**

此时不要先设计跨 API RHI。先使用原生 Vulkan 画出可靠的一帧。

### 子模块顺序

#### M3.1 Vulkan Instance

- Instance。
- Validation Layer。
- Debug Messenger。
- Surface。

#### M3.2 Vulkan Device

- Physical Device 选择。
- Queue Family。
- Logical Device。
- Graphics Queue。
- Present Queue。

#### M3.3 Vulkan Swapchain

- Surface Format。
- Present Mode。
- Extent。
- Swapchain Image。
- Image View。
- Resize 重建。

#### M3.4 Vulkan Command 与同步

- Command Pool。
- Command Buffer。
- Semaphore。
- Fence。
- 2 个 Frame in Flight。

#### M3.5 Vulkan Pipeline

- HLSL/GLSL 临时 Shader。
- Shader Module。
- Dynamic Rendering。
- Pipeline Layout。
- Graphics Pipeline。
- Viewport 和 Scissor。

### 推荐 FrameContext

```cpp
struct VulkanFrameContext
{
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore imageAvailable;
    VkSemaphore renderFinished;
    VkFence inFlightFence;
};
```

### 验收

- Vulkan 清屏。
- Vulkan 彩色三角形。
- Resize 正常。
- 最小化后恢复正常。
- 连续运行无验证错误。
- RenderDoc 能抓帧。

---

## 6.5 M4：最小 RHI

**建议时间：1～2 周**

只抽象 M3 中已经实际使用的功能。

### 公共类型

```cpp
enum class GraphicsAPI
{
    Vulkan,
    D3D12
};

enum class Format
{
    RGBA8_UNorm,
    RGBA8_sRGB,
    BGRA8_UNorm,
    RGBA16_Float,
    D32_Float,
    D24S8
};

enum class QueueType
{
    Graphics,
    Compute,
    Copy
};

enum class ResourceState
{
    Undefined,
    CopySource,
    CopyDestination,
    VertexBuffer,
    IndexBuffer,
    ConstantBuffer,
    RenderTarget,
    DepthWrite,
    DepthRead,
    ShaderResource,
    UnorderedAccess,
    Present
};
```

### 核心接口

```text
RHIDevice
RHISwapchain
RHICommandList
RHIBuffer
RHITexture
RHISampler
RHIShader
RHIPipeline
RHIFence
```

### Device 示例

```cpp
class RHIDevice
{
public:
    virtual ~RHIDevice() = default;

    virtual BufferHandle createBuffer(const BufferDesc&) = 0;
    virtual TextureHandle createTexture(const TextureDesc&) = 0;
    virtual PipelineHandle createGraphicsPipeline(
        const GraphicsPipelineDesc&) = 0;

    virtual void destroyBuffer(BufferHandle) = 0;
    virtual void destroyTexture(TextureHandle) = 0;
    virtual void waitIdle() = 0;
};
```

### CommandList 示例

```cpp
class RHICommandList
{
public:
    virtual void begin() = 0;
    virtual void end() = 0;

    virtual void transition(
        TextureHandle,
        ResourceState before,
        ResourceState after) = 0;

    virtual void beginRendering(const RenderingInfo&) = 0;
    virtual void endRendering() = 0;
    virtual void setPipeline(PipelineHandle) = 0;
    virtual void draw(uint32_t vertexCount, uint32_t firstVertex) = 0;
};
```

### 后端状态映射

```text
ResourceState
├─ Vulkan
│  ├─ VkImageLayout
│  ├─ VkAccessFlags2
│  └─ VkPipelineStageFlags2
└─ D3D12
   └─ D3D12_RESOURCE_STATES
```

### 验收

- Vulkan 三角形改为完全通过 RHI 绘制。
- Renderer 侧不包含 Vulkan 头文件。
- 上层不访问 Vulkan 帧索引或交换链对象。

---

## 6.6 M5：DirectX 12 后端

**建议时间：2～4 周**

### 子模块顺序

1. DXGI Factory。
2. Adapter 选择。
3. D3D12 Device。
4. Graphics Command Queue。
5. Swapchain。
6. RTV Descriptor Heap。
7. Command Allocator。
8. Graphics Command List。
9. Fence 和 Event。
10. Root Signature。
11. Pipeline State Object。
12. 三角形。

### 启动方式

```text
MyEditor.exe --rhi=vulkan
MyEditor.exe --rhi=d3d12
```

### 跨 API 概念对应

| 引擎概念 | Vulkan | DirectX 12 |
|---|---|---|
| Device | `VkDevice` | `ID3D12Device` |
| Queue | `VkQueue` | `ID3D12CommandQueue` |
| CommandList | `VkCommandBuffer` | `ID3D12GraphicsCommandList` |
| Pipeline | `VkPipeline` | `ID3D12PipelineState` |
| Pipeline Layout | `VkPipelineLayout` | Root Signature |
| Resource Binding | Descriptor Set | Descriptor Table/Heap |
| Buffer/Texture | `VkBuffer`/`VkImage` | `ID3D12Resource` |
| Barrier | Pipeline Barrier | Resource Barrier |
| Fence | Fence/Timeline Semaphore | `ID3D12Fence` |
| Swapchain | `VkSwapchainKHR` | `IDXGISwapChain` |
| Shader | SPIR-V | DXIL |

### 验收

- 同一程序可切换 Vulkan 和 DX12。
- 同一段上层代码画出相同三角形。
- D3D12 Debug Layer 无严重错误。
- PIX 能抓帧。

---

## 6.7 M6：统一 Shader 系统

**建议时间：1～2 周**

### 方案

以 HLSL 为唯一源码：

```text
HLSL
├─ DXC → DXIL → DX12
└─ DXC -spirv → SPIR-V → Vulkan
```

### 建议绑定约定

```hlsl
cbuffer FrameData : register(b0, space0);

Texture2D BaseColorTexture : register(t0, space1);
SamplerState MaterialSampler : register(s0, space1);

cbuffer ObjectData : register(b0, space2);
```

约定：

- `space0`：每帧和场景数据。
- `space1`：材质资源。
- `space2`：每对象数据。
- `space3`：Pass 私有资源。

### ShaderCompiler

```cpp
struct ShaderCompileDesc
{
    Path file;
    std::string entry;
    ShaderStage stage;
    std::vector<ShaderDefine> defines;
};

class ShaderCompiler
{
public:
    ShaderBinary compile(
        const ShaderCompileDesc&,
        GraphicsAPI target);
};
```

### 必须统一的约定

- 矩阵主序。
- 乘法顺序。
- NDC 深度范围。
- Vulkan Y 轴处理。
- Constant Buffer 对齐。
- Vertex Input。
- Descriptor/Register 映射。

### 验收

- 同一 HLSL 文件生成 DXIL 与 SPIR-V。
- 编译错误包含文件、行号和入口点。
- 两个后端的三角形使用同一 Shader 源码。

---

## 6.8 M7：Buffer、Texture 和上传系统

**建议时间：2～3 周**

### Buffer 实现顺序

1. Vertex Buffer。
2. Index Buffer。
3. Constant/Uniform Buffer。
4. Upload/Staging Buffer。
5. Storage Buffer。

### Texture 实现顺序

1. 2D Texture。
2. Texture View。
3. Sampler。
4. Mipmap。
5. Render Target。
6. Depth Texture。

### UploadContext

```cpp
class UploadContext
{
public:
    BufferHandle uploadBuffer(
        std::span<const std::byte> data,
        BufferUsage usage);

    TextureHandle uploadTexture(
        const TextureUploadDesc&);

    UploadTicket flush();
};
```

后端实现：

- Vulkan：Host Visible Staging Buffer → Copy → Device Local。
- DX12：Upload Heap → Copy → Default Heap。

### 延迟销毁

```text
资源请求销毁
→ 放入当前帧 Deletion Queue
→ 对应 GPU Fence 完成
→ 后端真正释放
→ Handle generation 更新
```

### 验收

- 两个后端绘制带索引的纹理立方体。
- 可更新每帧 Camera Buffer。
- Resize 和反复加载资源无生命周期错误。
- 资源销毁不会造成 GPU 崩溃。

---

## 6.9 M8：Renderer 高层

**建议时间：2～3 周**

### Renderer 职责

- 接收 RenderWorld。
- 更新每帧资源。
- 排序可见对象。
- 选择 Pipeline 和 Material。
- 录制 Pass。
- 提交 RHI 命令。

### RenderWorld

```cpp
struct RenderObject
{
    MeshHandle mesh;
    MaterialHandle material;
    Matrix4 worldMatrix;
    uint32_t objectID;
};

struct RenderWorld
{
    CameraData camera;
    std::vector<RenderObject> objects;
    std::vector<LightData> lights;
};
```

### 初版 Pass

```text
Depth / Shadow
→ Forward Opaque
→ Skybox
→ Transparent
→ Tone Mapping
→ UI
→ Present
```

第一版可简化为：

```text
Forward Opaque
→ UI
→ Present
```

### 验收

- 绘制多个不同 Transform 的物体。
- Mesh 资源可被多个实例复用。
- Material 可独立切换。
- 两个后端画面一致。
- Renderer 不依赖 ECS 类型。

---

## 6.10 M9：Asset 资源系统

**建议时间：2～3 周**

### 模块职责

- 识别资产。
- 导入源文件。
- 缓存 CPU 数据。
- 创建 GPU 资源。
- 记录资产依赖。
- 提供默认和错误资源。

### 数据流

```text
glTF / Texture Source
→ Importer
→ CPU Asset Data
→ Asset Cache
→ UploadContext
→ GPU Resource
→ AssetHandle / Render Handle
```

### 第一版资产

- Mesh。
- Texture。
- Material。
- Model。
- Scene。
- Shader。

### 默认资源

- 白色纹理。
- 黑色纹理。
- 默认法线纹理。
- 默认金属粗糙度纹理。
- 错误材质。
- 错误模型。

### 验收

- 加载 glTF/glb。
- 支持多 Primitive。
- Texture 和 Mesh 不重复加载。
- 资产失败时使用错误资源而不是崩溃。
- 两个后端加载同一资产。

---

## 6.11 M10：Scene 和组件

**建议时间：2～3 周**

### 最小组件

```cpp
IDComponent
NameComponent
TransformComponent
MeshRendererComponent
CameraComponent
LightComponent
```

### Scene 职责

- Entity 创建和删除。
- Component 增删。
- 父子层级。
- Transform 更新。
- Scene 序列化。
- 提取 RenderWorld。

### 数据边界

```text
Scene / ECS
→ RenderExtractionSystem
→ RenderWorld Snapshot
→ Renderer
```

### 序列化示例

```json
{
  "entities": [
    {
      "id": "uuid",
      "name": "Cube",
      "transform": {
        "position": [0, 0, 0],
        "rotation": [0, 0, 0, 1],
        "scale": [1, 1, 1]
      },
      "mesh_renderer": {
        "mesh": "Assets/Meshes/cube.glb",
        "material": "Assets/Materials/default.material.json"
      }
    }
  ]
}
```

### 验收

- 创建和删除 Entity。
- 添加和移除 Component。
- 保存和重新加载 Scene。
- 修改 Transform 后下一帧画面更新。
- Scene 层不包含 Vulkan 或 DX12 类型。

---

## 6.12 M11：基础光照和 PBR

**建议时间：4～6 周**

### 实现顺序

1. Depth Test。
2. Camera。
3. 法线变换。
4. Directional Light。
5. Blinn-Phong 验证。
6. Metallic-Roughness PBR。
7. Shadow Map。
8. HDR Render Target。
9. Tone Mapping。
10. IBL。
11. Bloom。
12. GPU Instancing。

### 为什么先实现 Blinn-Phong

Blinn-Phong 更容易验证：

- 法线是否正确。
- 灯光方向是否正确。
- 坐标空间是否统一。
- Material 和 Constant Buffer 是否正确绑定。

确认基础正确后再进入 PBR，调试成本更低。

### 验收

- glTF PBR 材质显示合理。
- 方向光和阴影工作正常。
- Tone Mapping 前后结果可对比。
- Vulkan 与 DX12 输出差异可解释。
- RenderDoc 和 PIX 中资源状态正确。

---

## 6.13 M12：Editor

**建议时间：3～5 周**

### 面板实现顺序

1. Game View。
2. Hierarchy。
3. Inspector。
4. Console。
5. Asset Browser。
6. Render Statistics。
7. Render Settings。

### 编辑能力

- 选择 Entity。
- 创建和删除 Entity。
- 编辑 Transform。
- 修改 Mesh 和 Material。
- 编辑 Camera 和 Light。
- 保存和加载 Scene。
- Transform Gizmo。
- Play/Stop。
- Vulkan/DX12 选择。

### Editor 与 Runtime 的边界

```text
Editor
→ 调用公开 Scene/Asset/Runtime API
→ 不直接修改 Renderer 后端对象
→ 不持有 Vk 或 D3D12 对象
```

### 验收

- 可以完整编辑并保存一个演示场景。
- 切换后端后场景仍能正常加载。
- Inspector 修改内容立即反映到 Game View。
- 编辑器关闭时资源按正确顺序释放。

---

## 6.14 M13：Render Graph

**建议时间：3～5 周；第一版编辑器完成后再做**

手工 Pass 链达到三到五个 Pass 后，再引入 Render Graph。

### Pass 声明示例

```cpp
graph.addPass(
    "ForwardOpaque",
    [&](PassBuilder& builder)
    {
        builder.write(sceneColor);
        builder.write(depth);
        builder.read(shadowMap);
    },
    [&](RHICommandList& cmd)
    {
        drawOpaqueObjects(cmd);
    });
```

### 第一版 Render Graph 职责

- Pass 拓扑排序。
- 资源读写声明。
- 自动 Barrier。
- 临时资源生命周期。
- Pass 调试名称。

暂不实现：

- 多 Queue 自动调度。
- Async Compute。
- 复杂资源别名。
- 自动并行录制。

### 验收

- Forward、Shadow 和 Post Process 使用 Graph 描述。
- Vulkan Layout 和 DX12 Resource State 自动转换。
- 能输出 Pass 和资源依赖图。

---

## 7. 引擎主循环

第一版保持单线程：

```cpp
while (!window.shouldClose())
{
    timer.tick();
    window.pollEvents();
    input.beginFrame();

    scene.update(timer.deltaTime());
    RenderWorld renderWorld = extractRenderWorld(scene);

    renderer.beginFrame();
    renderer.render(renderWorld);
    renderer.endFrame();
}
```

确认数据边界稳定后，再改为：

```text
Game Thread
→ 生成 RenderWorld Snapshot
→ 双缓冲或三缓冲交换
→ Render Thread
→ 录制并提交 GPU 命令
```

不要在第一版引擎中同时调试 ECS、多线程和双图形后端。

---

## 8. GPU 资源所有权

### 8.1 所有权建议

| 资源 | 所有者 |
|---|---|
| Window | Platform/Runtime |
| Device、Queue | RHI Device |
| Swapchain | RHI Swapchain |
| Command Context | 每帧 FrameContext |
| Mesh/Texture GPU 资源 | Renderer Resource Manager |
| CPU Asset | Asset Manager |
| Entity/Component | Scene |
| ImGui Backend | Editor Renderer Integration |

### 8.2 FrameContext

```cpp
struct FrameContext
{
    CommandAllocatorHandle commandAllocator;
    CommandListHandle commandList;
    FenceValue completionFence;
    DynamicBufferAllocator dynamicBuffer;
    DeletionQueue deletionQueue;
};
```

### 8.3 禁止行为

- GPU 未完成时覆盖当前帧 Constant Buffer。
- GPU 未完成时销毁 Buffer 或 Texture。
- Resize 时无条件销毁仍在使用的 Swapchain 资源。
- Asset Manager 直接持有原生 Vulkan/DX12 对象。

---

## 9. 测试和调试方案

### 9.1 每个后端必须开启

Vulkan：

- Validation Layer。
- Debug Utils Name。
- GPU Assisted Validation，按需开启。

DX12：

- D3D12 Debug Layer。
- GPU Based Validation，按需开启。
- DRED。

### 9.2 测试层级

#### 单元测试

- Handle Pool。
- UUID。
- Resource State 映射。
- Format 映射。
- Scene 序列化。
- Asset 路径和缓存键。

#### 集成测试

- Device 创建和销毁。
- Swapchain Resize。
- Buffer 上传。
- Texture 上传。
- Shader 编译。
- Scene 加载。

#### 视觉测试

- 三角形。
- 纹理立方体。
- glTF Helmet。
- 多灯光。
- Shadow。
- PBR 材质球。
- Tone Mapping。

### 9.3 每个图形里程碑的检查项

- Vulkan Validation 无新增错误。
- D3D12 Debug Layer 无新增错误。
- RenderDoc/PIX 可抓帧。
- Resize 20 次不崩溃。
- 最小化和恢复正常。
- 连续运行 10 分钟。
- 后端切换后资产结果一致。
- 程序退出无明显资源泄漏。

---

## 10. 28 周参考计划

| 周数 | 里程碑 | 可运行成果 |
|---|---|---|
| 1 | M0～M2 | 空窗口、输入、日志 |
| 2～3 | Vulkan 初始化 | Vulkan 稳定清屏 |
| 4 | Vulkan Pipeline | Vulkan 三角形 |
| 5～6 | M4 最小 RHI | 通过 RHI 画 Vulkan 三角形 |
| 7～9 | M5 DX12 | DX12 三角形 |
| 10 | M6 Shader | 同一 HLSL 支持两个后端 |
| 11～13 | M7 GPU 资源 | 纹理立方体 |
| 14～15 | M8 Renderer | 多物体和材质 |
| 16～17 | M9 Asset | 加载 glTF |
| 18～19 | M10 Scene | 保存和加载场景 |
| 20～21 | 基础光照 | Camera、Depth、方向光 |
| 22～24 | M11 PBR | PBR、Shadow、Tone Mapping |
| 25～27 | M12 Editor | 可编辑演示场景 |
| 28 | 稳定性和整理 | 双后端演示版本 |

如果每周少于 10 小时，将时间表整体乘以 1.5～2。

---

## 11. 三个版本里程碑

### V0.1：双后端渲染骨架

- Window。
- Vulkan Device/Swapchain。
- D3D12 Device/Swapchain。
- 最小 RHI。
- 同一 HLSL。
- 两个后端绘制纹理立方体。

### V0.2：可加载场景

- Buffer/Texture/Material。
- glTF。
- Asset Manager。
- Scene/Component。
- Camera。
- 多物体。
- 基础灯光。

### V0.3：可用编辑器原型

- PBR。
- Shadow。
- HDR/Tone Mapping。
- Hierarchy。
- Inspector。
- Asset Browser。
- Scene 保存加载。
- Vulkan/DX12 切换。

只有 V0.3 稳定后，再考虑动画、物理、脚本、Job System 和 Render Graph 的高级功能。

---

## 12. 常见风险与应对

### 风险 1：RHI 变成 Vulkan 包装层

表现：

- 公共接口使用 Vulkan 名称。
- Renderer 访问 Swapchain Image Index。
- 上层自己拼接 Vulkan Barrier 参数。

应对：

- 用 DX12 三角形尽早验证 RHI。
- RHI 描述“意图”，后端负责 API 映射。

### 风险 2：DX12 长期落后

表现：

- Vulkan 已经实现 PBR，DX12 仍停留在三角形。

应对：

- 每新增 Buffer、Texture、Descriptor、Pass 能力立即双后端验证。

### 风险 3：资源生命周期混乱

表现：

- 随机 GPU 崩溃。
- Resize 后设备丢失。
- 删除材质后偶发花屏。

应对：

- FrameContext。
- Fence 驱动的延迟销毁。
- Handle generation。
- 明确 CPU Asset 与 GPU Resource 所有权。

### 风险 4：Shader 两套代码逐渐分叉

应对：

- 单一 HLSL 源码。
- DXC 输出 DXIL/SPIR-V。
- 固定绑定和矩阵约定。
- 自动化 Shader 编译测试。

### 风险 5：过早开发高级系统

应对：

- 每项新功能必须服务于当前可运行里程碑。
- 双后端 glTF 场景未完成前，不开发网络、动画编辑器或 Job System。

---

## 13. 模块完成定义

一个模块只有满足以下条件才算完成：

- 公共职责明确。
- 初始化和销毁路径完整。
- 错误能被日志定位。
- 有最小测试或示例。
- 没有明显资源泄漏。
- Resize、退出等异常生命周期经过验证。
- 如果属于 RHI/Renderer，Vulkan 和 DX12 均通过。
- 文档记录输入、输出、所有者和线程归属。

“代码已经写完”不是完成标准，“调用成功并经过验证”才是。

---

## 14. 当前最应该开始的任务

如果今天开始新项目，依次执行：

1. 创建独立仓库和 CMake 工程。
2. 创建 `Engine` 库和 `Editor` 程序。
3. 接入日志、Assert 和 GLFW。
4. 打开空窗口并处理 Resize。
5. 开启 Vulkan Validation Layer。
6. 完成 Vulkan Instance、Device、Surface 和 Swapchain。
7. 先稳定清屏，再绘制三角形。
8. 用 RenderDoc 抓帧并修复验证错误。
9. 根据已经使用的功能提取最小 RHI。
10. 立即通过同一个 RHI 实现 DX12 三角形。

第一个关键目标是：

> 使用同一段上层代码和同一份 HLSL Shader，在 Vulkan 与 DX12 中绘制相同的三角形。

第二个关键目标是：

> 使用两个后端加载并绘制同一个 glTF 模型，且 Renderer 代码不包含任何 Vulkan 或 DX12 类型。

完成这两个目标后，双后端引擎的基础架构才真正成立。
