# Piccolo 源码学习路线

> 目标：系统理解 Piccolo 的运行时、场景、资源、渲染和编辑器架构，并为独立实现 Vulkan + DirectX 12 双后端游戏引擎建立知识基础。

## 1. 文档定位

这份文档解决三个问题：

1. Piccolo 源码应该按什么顺序阅读？
2. 每个阶段需要理解到什么程度？
3. 如何通过调试、修改和小实验确认自己真的掌握了？

这不是逐文件阅读清单。推荐采用“调用链 + 数据流 + 小实验”的方式学习，避免陷入局部实现。

自己的引擎如何分模块、如何实现，请阅读 [Vulkan + DX12 游戏引擎实现方案](./Vulkan_DX12_Engine_Implementation_Plan.md)。

---

## 2. 总体目标

完成本路线后，应当能够：

- 从 `main()` 开始解释 Piccolo 一帧的完整调用链。
- 解释 `World → Level → GObject → Component` 的对象组织方式。
- 解释 JSON 资产如何经过序列化和反射变成运行时对象。
- 解释逻辑世界如何向渲染世界提交数据。
- 区分 `RenderScene`、`RenderResource`、`RenderPipeline` 和 `RenderPass`。
- 解释 Vulkan 的设备、交换链、命令缓冲、描述符、同步和资源上传流程。
- 独立增加一个组件、一个资源类型和一个简单渲染 Pass。
- 判断 Piccolo 中哪些设计可以借鉴，哪些设计不适合直接扩展为 DX12 后端。

推荐周期为 **16 周**，每周投入 **10～15 小时**。如果 Vulkan 基础较弱，可延长到 20～24 周。

---

## 3. 开始前的知识准备

### 3.1 C++ 基础

至少需要掌握：

- 类、继承、虚函数和模板。
- RAII、智能指针和对象生命周期。
- STL 容器、迭代器和算法。
- Lambda、`std::function`、`std::optional`。
- 基本多线程概念。
- CMake 的 target、include、link 和构建配置。

不要求先精通：

- 模板元编程。
- 自定义内存分配器。
- 无锁数据结构。
- 编译器前端。

### 3.2 图形学基础

需要理解：

- 向量、矩阵、四元数。
- Model、View、Projection 变换。
- 顶点、索引、纹理、采样器。
- 深度测试、背面剔除、混合。
- Forward 与 Deferred Rendering 的基本区别。
- PBR、Shadow Map、HDR 和 Tone Mapping 的基本目标。

### 3.3 工具

建议准备：

- Visual Studio 调试器。
- Vulkan Validation Layer。
- RenderDoc。
- PIX。
- CMake。
- 支持 C++ 跳转和调用层级查看的 IDE。

---

## 4. 推荐学习方法

每个模块都按照下面的闭环学习。

### 4.1 第一次阅读：只看边界

回答：

- 谁创建这个模块？
- 谁调用它？
- 输入是什么？
- 输出是什么？
- 谁拥有资源？
- 谁负责销毁？

第一次不要深入每个 Vulkan 参数或算法细节。

### 4.2 第二次阅读：跟踪一条真实数据

例如选中一个带网格的游戏对象，跟踪：

```text
场景 JSON
→ GObject
→ MeshComponent
→ RenderSwapData
→ RenderEntity
→ RenderResource
→ RenderPass
→ GPU Draw Call
```

### 4.3 第三次阅读：运行时调试

在关键函数设置断点，记录：

- 调用顺序。
- 每帧调用次数。
- 关键对象地址及生命周期。
- 数据在哪一步发生变化。

### 4.4 第四次学习：做最小修改

推荐修改规模：

- 输出一条诊断日志。
- 增加一个字段。
- 修改一个参数。
- 增加一个简单组件。
- 增加一个后处理 Pass。

不要在第一次学习时重构整个模块。

### 4.5 学习笔记模板

每个模块使用同一份模板：

```text
模块名称：
入口文件：
主要职责：
初始化位置：
每帧入口：
输入数据：
输出数据：
拥有的资源：
依赖模块：
销毁顺序：
关键调用链：
最难理解的三个问题：
可以借鉴的设计：
不适合照搬的设计：
完成的小实验：
```

---

## 5. 阶段一：构建、启动和单帧主循环

**建议时间：第 1 周**

### 5.1 阅读文件

- [`README.md`](../README.md)
- [`build_windows.bat`](../build_windows.bat)
- [`engine/source/editor/source/main.cpp`](../engine/source/editor/source/main.cpp)
- [`engine/source/editor/source/editor.cpp`](../engine/source/editor/source/editor.cpp)
- [`engine/source/runtime/engine.cpp`](../engine/source/runtime/engine.cpp)
- [`engine/source/runtime/function/global/global_context.cpp`](../engine/source/runtime/function/global/global_context.cpp)

### 5.2 核心调用链

重点理解：

```text
main
→ PiccoloEngine::startEngine
→ Reflection::TypeMetaRegister::metaRegister
→ RuntimeGlobalContext::startSystems
→ Editor::run
→ PiccoloEngine::tickOneFrame
```

单帧流程：

```text
logicalTick
→ calculateFPS
→ swapLogicRenderData
→ rendererTick
→ physics debug render（可选）
→ pollEvents
```

### 5.3 需要回答的问题

- 为什么系统初始化存在固定顺序？
- 为什么销毁顺序大致与初始化顺序相反？
- `logicalTick()` 与 `rendererTick()` 为什么分开？
- 当前逻辑与渲染是否真正运行在两个线程？
- `delta_time` 如何计算？第一帧是否可能异常？

### 5.4 实践任务

- 成功编译并启动编辑器。
- 在 `tickOneFrame()`、`logicalTick()` 和 `rendererTick()` 设置断点。
- 记录启动期间所有系统的初始化顺序。
- 增加一条每 300 帧输出一次的诊断信息。

### 5.5 验收标准

- 能不看源码画出 Piccolo 单帧时序。
- 能指出窗口事件处理在帧循环中的位置。
- 能解释逻辑和渲染数据为什么需要交换边界。

---

## 6. 阶段二：Core、配置和文件系统

**建议时间：第 2 周**

### 6.1 阅读范围

```text
engine/source/runtime/core/
├─ base/
├─ color/
├─ log/
├─ math/
└─ meta/

engine/source/runtime/platform/
├─ file_service/
└─ path/

engine/source/runtime/resource/config_manager/
```

优先阅读：

- [`engine/source/runtime/core/log/log_system.cpp`](../engine/source/runtime/core/log/log_system.cpp)
- [`engine/source/runtime/resource/config_manager/config_manager.cpp`](../engine/source/runtime/resource/config_manager/config_manager.cpp)
- [`engine/source/runtime/platform/file_service/file_service.cpp`](../engine/source/runtime/platform/file_service/file_service.cpp)
- [`engine/configs/development/PiccoloEditor.ini`](../engine/configs/development/PiccoloEditor.ini)

### 6.2 学习重点

- 配置文件如何确定引擎根路径和资产路径。
- 日志和断言如何被其他模块使用。
- 数学类型采用什么坐标和矩阵约定。
- 文件路径在 Windows、Linux、macOS 上如何处理。

### 6.3 实践任务

- 增加一个配置项并在启动时读取。
- 使用文件系统模块读取一个文本文件。
- 验证矩阵乘法顺序和坐标系约定。

### 6.4 验收标准

- 能解释配置、文件和资产路径之间的关系。
- 能写出 Piccolo 使用的变换组合顺序。

---

## 7. 阶段三：资产、JSON、反射与代码生成

**建议时间：第 3～4 周**

### 7.1 阅读文件

- [`engine/source/runtime/resource/asset_manager/asset_manager.cpp`](../engine/source/runtime/resource/asset_manager/asset_manager.cpp)
- [`engine/source/runtime/core/meta/json.h`](../engine/source/runtime/core/meta/json.h)
- [`engine/source/runtime/core/meta/serializer/serializer.cpp`](../engine/source/runtime/core/meta/serializer/serializer.cpp)
- [`engine/source/runtime/core/meta/reflection/reflection.cpp`](../engine/source/runtime/core/meta/reflection/reflection.cpp)
- [`engine/source/runtime/core/meta/reflection/reflection_register.cpp`](../engine/source/runtime/core/meta/reflection/reflection_register.cpp)
- [`engine/source/meta_parser/README.md`](../engine/source/meta_parser/README.md)
- [`engine/template/`](../engine/template)
- [`engine/asset/world/hello.world.json`](../engine/asset/world/hello.world.json)
- [`engine/asset/level/1-1.level.json`](../engine/asset/level/1-1.level.json)

### 7.2 核心数据流

```text
C++ 类型及反射标记
→ Meta Parser
→ Mustache 模板
→ 生成反射和序列化代码
→ 编译进 Runtime
→ AssetManager 读取 JSON
→ Serializer 构造运行时对象
```

### 7.3 学习重点

- Piccolo 为什么需要代码生成？
- 类型、字段和基类信息如何登记？
- 资产类型如何通过模板完成序列化？
- 反射生成代码在什么时候注册和注销？
- JSON 数据缺失或版本变化时如何处理？

### 7.4 实践任务

增加一个简单资源类型，例如：

```cpp
struct GameplaySettings
{
    float gravity;
    float movement_speed;
};
```

完成：

- 加入反射和序列化标记。
- 从 JSON 读取。
- 修改后保存或输出验证。

### 7.5 验收标准

- 能解释“编译期代码生成”和“运行时反射”的分工。
- 能从一个 JSON 字段追踪到对应的 C++ 成员。
- 能独立增加一种可序列化资源。

---

## 8. 阶段四：World、Level、Object 和 Component

**建议时间：第 5～6 周**

### 8.1 阅读顺序

1. [`world_manager.cpp`](../engine/source/runtime/function/framework/world/world_manager.cpp)
2. [`level.cpp`](../engine/source/runtime/function/framework/level/level.cpp)
3. [`object.cpp`](../engine/source/runtime/function/framework/object/object.cpp)
4. [`component.h`](../engine/source/runtime/function/framework/component/component.h)
5. [`transform_component.cpp`](../engine/source/runtime/function/framework/component/transform/transform_component.cpp)
6. [`mesh_component.cpp`](../engine/source/runtime/function/framework/component/mesh/mesh_component.cpp)
7. [`camera_component.cpp`](../engine/source/runtime/function/framework/component/camera/camera_component.cpp)
8. [`rigidbody_component.cpp`](../engine/source/runtime/function/framework/component/rigidbody/rigidbody_component.cpp)

### 8.2 对象关系

```text
WorldManager
└─ Current World
   └─ Level
      └─ GObject
         ├─ TransformComponent
         ├─ MeshComponent
         ├─ CameraComponent
         ├─ RigidBodyComponent
         └─ 其他 Component
```

### 8.3 学习重点

- 对象 ID 如何生成和回收？
- Component 在哪里创建和 tick？
- Transform 数据如何传递给其他组件？
- Level 加载、切换和清理的生命周期是什么？
- 编辑器模式和运行模式对 Component tick 有什么影响？

### 8.4 实践任务

增加一个 `RotatorComponent`：

- JSON 中保存旋转速度。
- 每帧更新 Transform。
- 在编辑器和运行时验证。

### 8.5 验收标准

- 能解释对象、组件和资源描述之间的差别。
- 能独立增加并序列化一种 Component。
- 能说明 Piccolo 组件模型与数据导向 ECS 的差别。

---

## 9. 阶段五：逻辑世界与渲染世界的数据交换

**建议时间：第 7 周**

### 9.1 阅读文件

- [`render_swap_context.h`](../engine/source/runtime/function/render/render_swap_context.h)
- [`render_swap_context.cpp`](../engine/source/runtime/function/render/render_swap_context.cpp)
- [`render_system.cpp`](../engine/source/runtime/function/render/render_system.cpp)
- [`render_entity.h`](../engine/source/runtime/function/render/render_entity.h)
- [`render_scene.cpp`](../engine/source/runtime/function/render/render_scene.cpp)
- [`render_resource.cpp`](../engine/source/runtime/function/render/render_resource.cpp)

### 9.2 核心数据流

```text
Game Object / Component
→ 构造 GameObjectDesc
→ 写入 RenderSwapData
→ swapLogicRenderData
→ RenderSystem::processSwapData
→ RenderEntity
→ RenderScene
→ RenderResource
```

### 9.3 必须区分的概念

| 概念 | 主要职责 |
|---|---|
| GObject | 逻辑世界对象 |
| GameObjectDesc | 提交给渲染侧的数据描述 |
| RenderEntity | 渲染侧轻量实例 |
| RenderScene | 可渲染实体、灯光和可见性 |
| RenderResource | 网格、纹理、材质及 GPU 缓冲 |
| RenderSwapContext | 逻辑和渲染数据的交换边界 |

### 9.4 实践任务

- 修改一个对象的位置，逐步观察数据如何传到 `RenderEntity`。
- 记录网格第一次出现和后续帧更新的差别。
- 观察对象删除时 GPU 资源是否立即销毁。
- 为交换数据增加一条统计信息，例如本帧新增、更新、删除对象数。

### 9.5 验收标准

- 能画出一个网格对象从 JSON 到 Draw Call 的完整数据流。
- 能解释为什么 Renderer 不应该直接遍历全部游戏组件。
- 能解释交换上下文与真正双线程渲染之间还缺少什么。

---

## 10. 阶段六：RenderSystem、RenderScene 和 RenderResource

**建议时间：第 8～9 周**

### 10.1 阅读顺序

1. [`render_system.cpp`](../engine/source/runtime/function/render/render_system.cpp)
2. [`render_camera.cpp`](../engine/source/runtime/function/render/render_camera.cpp)
3. [`render_scene.cpp`](../engine/source/runtime/function/render/render_scene.cpp)
4. [`render_resource_base.cpp`](../engine/source/runtime/function/render/render_resource_base.cpp)
5. [`render_resource.cpp`](../engine/source/runtime/function/render/render_resource.cpp)
6. [`render_mesh.h`](../engine/source/runtime/function/render/render_mesh.h)
7. [`render_type.h`](../engine/source/runtime/function/render/render_type.h)

### 10.2 每帧渲染准备

```text
processSwapData
→ RHI::prepareContext
→ updatePerFrameBuffer
→ updateVisibleObjects
→ preparePassData
→ RenderPipeline
```

### 10.3 学习重点

- CPU 网格如何上传为 GPU Buffer？
- 材质和纹理如何缓存？
- 每帧常量、每材质数据、每对象数据如何组织？
- Ring Buffer 为什么需要按帧重置偏移？
- 可见性计算发生在哪里？
- Mesh ID、Instance ID 和 GObject ID 如何映射？

### 10.4 实践任务

- 增加每帧 Draw Call 和可见对象数量统计。
- 找出默认纹理和默认材质的使用位置。
- 修改摄像机远近裁剪面，验证剔除和深度效果。

### 10.5 验收标准

- 能区分 CPU 资源、GPU 资源和渲染实例。
- 能解释一份 Mesh 资源如何被多个对象复用。
- 能解释每帧动态数据为什么不能随意覆盖。

---

## 11. 阶段七：Vulkan RHI

**建议时间：第 10～12 周**

### 11.1 阅读顺序

1. [`rhi.h`](../engine/source/runtime/function/render/interface/rhi.h)
2. [`rhi_struct.h`](../engine/source/runtime/function/render/interface/rhi_struct.h)
3. [`vulkan_rhi.h`](../engine/source/runtime/function/render/interface/vulkan/vulkan_rhi.h)
4. [`vulkan_rhi.cpp`](../engine/source/runtime/function/render/interface/vulkan/vulkan_rhi.cpp)
5. [`vulkan_util.cpp`](../engine/source/runtime/function/render/interface/vulkan/vulkan_util.cpp)
6. [`vulkan_vma.cpp`](../engine/source/runtime/function/render/interface/vulkan/vulkan_vma.cpp)

### 11.2 按功能阅读

不要从头到尾逐行读 `vulkan_rhi.cpp`。按下面顺序定位函数：

1. Instance 与 Validation Layer。
2. Surface 与 Physical Device。
3. Logical Device 与 Queue。
4. Swapchain 与 Image View。
5. Command Pool 与 Command Buffer。
6. Semaphore 与 Fence。
7. Buffer、Image 与 VMA。
8. Descriptor。
9. Pipeline。
10. Acquire、Submit 与 Present。
11. Swapchain 重建。
12. 清理和销毁。

### 11.3 一帧同步流程

```text
等待当前帧 Fence
→ 获取 Swapchain Image
→ 重置 Fence / Command Pool
→ 录制 Command Buffer
→ Queue Submit
→ Present
→ 移动 Current Frame Index
```

### 11.4 实践任务

- 开启 Validation Layer，并清理能够复现的验证错误。
- 用 RenderDoc 抓取一帧。
- 找到最终 Swapchain Image 的写入位置。
- 记录一张纹理从 CPU 像素到 Shader Resource 的全部状态变化。
- 解释 Resize 后哪些对象必须重建、哪些不需要。

### 11.5 关于 DX12 的重要观察

Piccolo 有 RHI 分层思想，但当前接口仍明显偏向 Vulkan：

- `rhi.h` 直接包含 GLFW Vulkan 和 VMA 头文件。
- 部分结构和函数形状与 Vulkan API 高度一致。
- 上层代码存在对 `VulkanRHI` 的向下转换。
- Viewport、帧索引、Swapchain Image Index 等实现细节泄漏到上层。
- Render Pass 和 Descriptor Set 的组织方式偏向 Vulkan。

结论：

> 可以学习 Piccolo 的 RHI 分层目标，但不建议直接在当前 RHI 下实现 DX12。自己的引擎应重新定义后端无关的资源、状态、命令和同步模型。

### 11.6 验收标准

- 能解释 Vulkan 初始化和单帧提交的完整顺序。
- 能解释 Fence 与 Semaphore 的职责区别。
- 能解释 Buffer/Image 上传和 Barrier。
- 能列出至少五处妨碍 DX12 接入的 Vulkan 耦合。

---

## 12. 阶段八：RenderPipeline 与 RenderPass

**建议时间：第 13～14 周**

### 12.1 阅读顺序

- [`render_pipeline.cpp`](../engine/source/runtime/function/render/render_pipeline.cpp)
- [`render_pass_base.cpp`](../engine/source/runtime/function/render/render_pass_base.cpp)
- [`render_pass.cpp`](../engine/source/runtime/function/render/render_pass.cpp)

Pass 建议从简单到复杂：

1. [`tone_mapping_pass.cpp`](../engine/source/runtime/function/render/passes/tone_mapping_pass.cpp)
2. [`color_grading_pass.cpp`](../engine/source/runtime/function/render/passes/color_grading_pass.cpp)
3. [`fxaa_pass.cpp`](../engine/source/runtime/function/render/passes/fxaa_pass.cpp)
4. [`directional_light_pass.cpp`](../engine/source/runtime/function/render/passes/directional_light_pass.cpp)
5. [`point_light_pass.cpp`](../engine/source/runtime/function/render/passes/point_light_pass.cpp)
6. [`pick_pass.cpp`](../engine/source/runtime/function/render/passes/pick_pass.cpp)
7. [`main_camera_pass.cpp`](../engine/source/runtime/function/render/passes/main_camera_pass.cpp)
8. [`particle_pass.cpp`](../engine/source/runtime/function/render/passes/particle_pass.cpp)

### 12.2 学习重点

- 每个 Pass 的输入和输出附件是什么？
- Pass 初始化与每帧绘制分别做什么？
- Pipeline 如何确定 Pass 顺序？
- Descriptor Layout 为什么存在跨 Pass 依赖？
- Swapchain 重建后哪些 Pass 资源需要更新？
- Forward 和 Deferred 路径共享了什么？

### 12.3 实践任务

新增一个简单后处理 Pass，例如：

- 灰度。
- 颜色反相。
- 深度可视化。
- 简单边缘检测。

任务必须包括：

- Shader。
- 输入附件。
- Descriptor。
- Pipeline。
- Draw。
- Resize 后资源更新。

### 12.4 验收标准

- 能画出全部 Pass 的执行顺序和附件流向。
- 能独立添加一个全屏后处理 Pass。
- 能解释手工 Pass 链和 Render Graph 的差别。

---

## 13. 阶段九：编辑器、输入、物理、动画和粒子

**建议时间：第 15～16 周**

这些模块放在最后阅读，因为它们依赖前面的运行时、资源、场景和渲染知识。

### 13.1 编辑器

阅读：

- [`editor_ui.cpp`](../engine/source/editor/source/editor_ui.cpp)
- [`editor_scene_manager.cpp`](../engine/source/editor/source/editor_scene_manager.cpp)
- [`editor_input_manager.cpp`](../engine/source/editor/source/editor_input_manager.cpp)
- [`axis.cpp`](../engine/source/editor/source/axis.cpp)

理解：

- 编辑器与运行时如何共享引擎。
- Viewport 如何嵌入 UI。
- 实体选择和 Pick Pass 如何配合。
- Transform Gizmo 如何更新游戏对象。

### 13.2 物理

阅读 `engine/source/runtime/function/physics/` 和 Rigidbody Component，重点关注：

- 物理世界初始化。
- Rigidbody 创建。
- Transform 同步方向。
- 固定时间步。
- Debug Draw。

### 13.3 动画

阅读 `engine/source/runtime/function/animation/`，重点关注：

- Skeleton。
- Animation Clip。
- Blend。
- 骨骼矩阵如何提交给渲染器。

### 13.4 粒子

阅读：

- `engine/source/runtime/function/particle/`
- `engine/source/runtime/function/render/passes/particle_pass.cpp`
- `engine/shader/glsl/particle_*.comp`

重点理解 Compute 和 Graphics Pass 之间的数据依赖。

### 13.5 验收标准

- 能解释编辑器如何驱动 Runtime，而不是完全独立运行。
- 能说明物理、动画和粒子数据最终如何影响渲染。
- 能选择一个子系统完成可观察的小改动。

---

## 14. 16 周计划

| 周数 | 学习主题 | 必须产出 |
|---|---|---|
| 1 | 构建、入口、主循环 | 单帧时序图 |
| 2 | Core、配置、文件 | 模块依赖笔记 |
| 3 | Asset、JSON | 一条资产加载调用链 |
| 4 | 反射、代码生成 | 新增可序列化资源 |
| 5 | World、Level、Object | 对象生命周期图 |
| 6 | Component | 新增 Rotator Component |
| 7 | Logic/Render Swap | 完整网格数据流图 |
| 8 | RenderScene | 可见对象统计 |
| 9 | RenderResource | GPU 资源生命周期笔记 |
| 10 | Vulkan 初始化 | 初始化调用链 |
| 11 | Vulkan 资源和 Descriptor | 纹理上传状态图 |
| 12 | Vulkan 同步与 Swapchain | 单帧同步图 |
| 13 | Pipeline 和简单 Pass | Pass 输入输出图 |
| 14 | Main Camera Pass | 新增后处理 Pass |
| 15 | Editor | 选择、视口、Gizmo 调用链 |
| 16 | 自选子系统和总结 | 架构总结及改进清单 |

---

## 15. 最终综合练习

完成下面三个任务，即可认为 Piccolo 第一轮学习结束。

### 15.1 增加一个组件

示例：`RotatorComponent` 或 `AutoMoveComponent`。

要求：

- 可序列化。
- 可在场景 JSON 中配置。
- 运行时更新 Transform。
- 编辑器模式行为明确。

### 15.2 增加一个后处理 Pass

要求：

- 有独立 Shader。
- 能读取前一个 Pass 的结果。
- 支持窗口 Resize。
- Validation Layer 无新增严重错误。

### 15.3 写一份架构复盘

至少回答：

- Piccolo 最值得学习的三个设计是什么？
- 最难维护的三个耦合点是什么？
- 如果重新设计 RHI，哪些 API 必须变化？
- 如果引入渲染线程，数据所有权应如何调整？
- 如果加入 DX12，Shader、Descriptor、Barrier 如何统一？

---

## 16. 学习过程中的停止条件

遇到下面情况时，不要继续向后读：

- 不能解释当前模块由谁创建和销毁。
- 不能画出当前模块的输入输出。
- 只认识 Vulkan 函数名，但不知道它解决什么问题。
- 修改代码后无法预测影响范围。
- 没有用断点或抓帧工具验证自己的推断。

回到上一阶段，用一个更小的实验补齐理解。

---

## 17. 阅读优先级

如果时间有限，按以下优先级：

### 17.1 必须掌握

- 主循环和系统生命周期。
- World/Level/Object/Component。
- Asset、序列化和反射。
- Logic/Render Swap。
- RenderScene、RenderResource。
- Vulkan RHI。
- RenderPipeline 和 Main Camera Pass。

### 17.2 第二优先级

- 编辑器。
- Pick Pass。
- Debug Draw。
- 物理。

### 17.3 可以后置

- 完整动画系统。
- 粒子系统。
- Lua。
- Meta Parser 内部的 Clang AST 细节。

---

## 18. 完成定义

当你能够在不依赖源码搜索的情况下，讲清楚以下故事，学习路线才算真正完成：

> Piccolo 启动后如何创建所有系统；一个场景 JSON 如何生成对象和组件；Mesh Component 如何把逻辑数据提交给渲染侧；渲染侧如何加载和缓存资源、计算可见对象、更新每帧数据；各个 Pass 如何录制 Vulkan 命令并提交到交换链；编辑器又如何选择和修改场景对象。

完成后进入 [Vulkan + DX12 游戏引擎实现方案](./Vulkan_DX12_Engine_Implementation_Plan.md)，用独立项目重建这条链路。
