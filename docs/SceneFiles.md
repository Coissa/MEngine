# 场景文件与资源

启动场景由 `Assets/MEngine.project` 的 `startup_scene` 指定，路径相对于项目 Assets 目录。命令行 `--scene <文件>` 可以覆盖它。编辑器的 Open 使用文件选择窗口，Save 保存当前文件，Save As 另存文件，New 创建空场景。

随项目提供：

- `Scenes/RendererLab.mscene`：默认启动场景，来自 RendererLab 的骷髅头展示场景，包含 50 个对象、20 个网格资产及 15 个材质。包含地面、立柱与球体、骷髅头、围栏、水面、树木、曲面、冰面和两名士兵。
- `Scenes/RendererArchitectureDemo.mscene`：单个棋盘格立方体的最小示例。Play 后由 Rotator 组件驱动旋转。
- `Scenes/PBRMaterialLab.mscene`：金属度/粗糙度材质球阵列和 HDR 自发光球。
- `Scenes/TwoCubes.mscene`：两个物体和两个材质，使用另一组场景内资源编号，方便验证文件切换。
- `Scenes/Default.mscene`：原来的双立方体法线调试场景，也提供 v1–v3 场景的旧编号映射。

所有场景共用原有天空盒资源、曝光和 Shader。场景曝光统一作用于线性 HDR 网格和天空，最终只进行一次显示映射。

RendererLab 场景保留静态内容和布局，已开启 PBR、天空 IBL、方向光级联阴影、点光/聚光及砖块/地砖法线与粗糙度图。士兵的两个姿态已烘焙为普通网格，树木使用固定朝向的透明裁剪面片，水和冰使用不透明近似。平面反射、SSAO、水波、透射和实时骨骼动画尚未实现。资源来源见 [RendererLab/SOURCE.md](../Assets/RendererLab/SOURCE.md)。

可从原项目重新导出；该步骤只用于资产制作，运行编辑器不需要原项目目录：

```powershell
python Tools/ExportRendererLabScene.py --source E:\d3d12study\RendererLab
```

## .mscene v5

保留旧版对象/父子层级/组件结构和资源引用表；v5 在 v4 的 `render_settings` 后加入环境、阴影与局部灯。v1–v4 继续可读，旧版默认关闭新增环境照明和阴影，局部灯为空：

```text
MENGINE_SCENE 5
scene_state 0
render_settings -0.4 0.8 -0.7 2.4 0.03 1.2 0
lighting_settings 1 1 1 1 0 1 80
local_lights 1
light 0 0 3 -2 10 0 -1 0 20 1 0.7 0.4 20 35
mesh_assets 1
1 5f7a3956-4d69-562b-9376-2abfaeda2313
material_assets 1
1 cd2a1dd4-c9a0-50be-b1bd-02f469fb1972
object_count 1
object 1 "Cube"
parent 0
component_count 2
component "MEngine.Runtime.TransformComponent" 1 17
0 0 0 0 0 0 1 1 1
component "MEngine.Runtime.MeshRendererComponent" 1 5
1 1 1
end_object
end_scene
```

`render_settings` 的字段依次为：指向光源的世界空间方向 XYZ、方向光强度、环境亮度、曝光、法线调试开关（0/1）。光照采用金属度/粗糙度 GGX PBR；ambient 暂为漫反射环境填充。HDR 场景与天空统一曝光映射为 `1 - exp(-color × exposure)`，再进行分段 sRGB 显示转换。法线/Cluster 调试颜色绕过色调映射。

`lighting_settings` 依次为方向光颜色 RGB、环境强度、环境旋转角度（度）、方向光阴影开关、阴影距离。`local_lights` 后每条 `light` 依次为类型（0 点光 / 1 聚光）、位置 XYZ、范围、发射方向 XYZ、强度、颜色 RGB、内锥半角、外锥半角（度）。最多 64 盏，聚光需满足 0 ≤ 内角 < 外角 < 90°；局部光当前不投影。光源使用世界空间值，可在 Scene → Lighting 编辑并保存。

上例展示仍可读取的组件 payload v1。现在保存内置组件时写入 payload v2：通过反射按字段名序列化，形如 `"MEngine.Runtime.CameraComponent" { "primary" = 1 ; ... }`；组件记录仍携带独立版本和 payload 字节数，场景外层版本为 v5。字段顺序不影响读取。

资源表每项为 `场景内编号 AssetId`。MeshRenderer 的 mesh、material 字段保留场景内资源编号（v1 的三个值为 `mesh编号 material编号 visible`），其中前两个编号分别指向当前场景的资源表；它们不再依赖启动代码创建某个固定资源。相同编号可以在不同场景关联不同资产。纹理由材质资产引用，不需要出现在对象组件中。

组件头依次记录稳定类型名、组件版本和 payload 字节数。组件 payload 为 UTF-8 文本，字节数不包含尾部换行。支持 LF 和 CRLF 文件。修改 payload 后需要更新字节数；通过编辑器保存会自动计算。

已有组件字段顺序：

| 组件 | payload |
| --- | --- |
| TransformComponent | 平移 XYZ、旋转 XYZ（度）、缩放 XYZ |
| MeshRendererComponent | mesh 编号、material 编号、visible |
| CameraComponent | 垂直视场角（度）、近平面、远平面、primary |
| RotatorComponent | 每秒旋转角度 XYZ |

Rotator 只在 Play/PIE 中更新，不改变编辑器场景。Demo 使用父对象保存固定 X 倾角、子对象绕 Y 旋转，从而保留原示例的 `RotationY × RotationX` 顺序。

## 项目资产

`AssetRegistry.mregistry` 将 AssetId 关联到资产类型和项目相对路径，ID 沿用项目现有的 namespace + AssetKey 派生规则。新增资源需要在注册表中登记，再将其 ID 加入场景的资源表。

编辑器按当前场景的引用加载资源：有 Cook 路径时使用现有 ResourceManager；没有 Cook 路径时，网格使用现有 glTF 导入器，纹理使用现有 PNG/JPEG/HDR 导入器。源材质 `.material` 使用 JSON，例如：

```json
{
  "base_color": [1, 1, 1, 1],
  "base_color_texture": "9f770a06-e60d-5695-9763-2d594f7baffc",
  "uv_scale": [1, 1],
  "alpha_cutoff": 0,
  "filter": "linear",
  "address": "clamp"
}
```

上面的纹理 ID 对应随项目提供的棋盘纹理。`base_color_texture` 可省略，此时使用白色纹理；filter 支持 linear/nearest，address 支持 repeat/clamp。材质支持金属度/粗糙度 PBR、法线、自发光与 AO；颜色纹理与数据纹理分别选择 sRGB 和线性采样。完整字段与流程见 [Materials.md](Materials.md)。每个场景 mesh 引用把源资产的节点变换烘焙到几何中，使用场景指定的单个材质；保持当前每网格最多 65536 个顶点的 16 位索引契约。

`uv_scale` 控制纹理重复次数，需配合 repeat 采样；地面使用 `[12, 12]`。`alpha_cutoff` 为透明裁剪阈值，默认 0，树木和铁丝网使用 0.1；被裁剪的像素不写入颜色、深度或速度。水面与冰面使用不透明 PBR 材质与天空 IBL，不启用透明排序或平面反射 Pass。

切换场景先准备新 World 和 CPU 资源，成功后等待 GPU 空闲，再替换 Renderer 与拾取包围盒。资源加载失败时继续保留原场景。保存包含资产引用、曝光和光照设置；PIE 也复制这些配置。

## 绘制顺序

```text
Cull（可选）
  → 方向光四级阴影（可选，与主绘制共用 Alpha Mask）
  → 场景网格 HDR + 深度 + 速度（方向光 / 局部灯 / IBL）
  → 当前天空盒填充 HDR 背景 + 天空速度
  → ResolveTAA（可选，在线性 HDR 中）
  → PostProcess::ToneMapScene（曝光、色调映射、sRGB）
  → MotionVector 可视化（可选）
  → EditorDebugPass（Gizmo，读取场景深度）
  → UIPass（编辑器窗口合成）
```

EditorDebugPass 在后处理之后绘制，不进入 TAA 历史，也不受场景曝光影响。Renderer 和 Pass 都不读取场景文件或 World，只消费加载后的资源与 RenderFrame。
