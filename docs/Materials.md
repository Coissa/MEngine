# 材质、PBR 与色彩空间

当前采用 metallic–roughness PBR：GGX 法线分布、Smith 高度相关可见性、Schlick Fresnel 和能量分配的漫反射。直接光包含方向光、点光和聚光；天空同时参与漫反射与镜面 IBL，方向光使用四级级联阴影。

## 材质文件

源 `.material` 和 Cook 文件都先转换成 MaterialAsset，再传给 Renderer，避免两条路径丢失不同属性。Cook 材质写为 payload v2，保留 v1 读取能力。

```json
{
  "base_color": [0.8, 0.2, 0.1, 1],
  "metallic": 0,
  "roughness": 0.45,
  "emissive": [0, 0, 0],
  "uv_scale": [1, 1],
  "alpha_cutoff": 0,
  "normal_scale": 1,
  "occlusion_strength": 1,
  "filter": "linear",
  "address": "repeat"
}
```

可选纹理字段为 `base_color_texture`、`metallic_roughness_texture`、`normal_texture`、`emissive_texture` 和 `occlusion_texture`，值填写资产注册表中纹理的 AssetId，并在材质依赖中登记。基础色/自发光系数使用线性数值，基础色分量在 0–1 内，自发光允许大于 1。自定义材质未指定 metallic 时默认非金属；glTF 导入保留 glTF 的默认 metallicFactor=1。

| 参数 | 用法 |
|---|---|
| base_color_texture | RGB 按 sRGB 解码；Alpha 保持线性，与 base_color 相乘 |
| metallic_roughness_texture | 线性数据，G 乘 roughness，B 乘 metallic |
| normal_texture | 线性切线空间法线，绿色为 +Y；normal_scale 缩放 XY 分量 |
| emissive_texture | RGB 按 sRGB 解码，与线性 emissive 相乘 |
| occlusion_texture | 线性 R 通道，按 occlusion_strength 调整；影响环境漫反射与粗糙度相关的镜面遮蔽近似，不影响直接光或自发光 |
| alpha_cutoff | 大于 0 时产生 Alpha Mask；当前不包含透明混合材质 |

相同 RGBA8 GPU 纹理可以同时具有线性和 sRGB SRV，按材质槽选择，确保 sRGB 解码发生在硬件纹理过滤之前。浮点 HDR 纹理保持线性。缺少的纹理使用白色中性值，缺少法线贴图时使用几何法线。

导入和 `GenerateTextureMips` 都在线性空间过滤 sRGB 的 RGB 通道，Alpha 保持线性；法线 Mip 过滤后重新归一化，非 2 次幂尺寸采用面积加权，边缘不会丢失。SceneAssetLoader 按颜色、数据、法线三种用途复用纹理；同一个 AssetId 用于不同用途且需要不同 Mip 内容时分开上传。源文件生成完整 Mip，Cook 文件在用途不符或缺少 Mip 时从基础层重建；法线用途始终进行归一化重建。

法线贴图目前通过屏幕导数重建切线基，支持镜像 UV，不改变现有顶点/Cook 网格格式。它尚不保证与所有烘焙工具的 MikkTSpace 切线完全一致；后续需要精确匹配烘焙资产时再扩展导入切线。

高光使用屏幕空间法线方差过滤：以像素内的细粒度法线导数估计不可分辨的表面变化，在 GGX 的 α² 空间增加受限滤波宽度，再还原感知粗糙度。方向光、局部灯和 IBL 共用过滤结果。平坦表面保留原粗糙度，头骨牙齿等亚像素曲面不再直接产生过窄的高光；材质资产参数不变。该处理与 TAA 配合，仍保留正常几何边缘的时域采样变化。[方法背景](https://google.github.io/filament/main/materials.html#anti-aliasing:specularantialiasing)。

## 默认 RendererLab 场景

默认启动的 `RendererLab.mscene` 已为全部 15 个材质显式配置金属度与粗糙度。以下为基于当前资产用途的外观初值，不是测量数据；导出脚本也保存同样的配置，重新导出会保留它们。

| 材质 | metallic | roughness | 表面用途 |
|---|---|---|---|
| Bricks | 0 | 0.90 | 粗糙砖石 |
| GroundTiles / PatchTiles | 0 | 0.45 | 较光滑地砖 |
| Polished | 1 | 0.22 | 骷髅与立柱球体使用的抛光金属 |
| Fence | 1 | 0.50 | 金属丝网，保留原 Alpha Mask |
| Tree0 / Tree1 / Tree2 | 0 | 0.85 | 粗糙植物面片，保留原 Alpha Mask |
| soldier_head | 0 | 0.60 | 皮肤的单层 PBR 近似 |
| soldier_jacket | 0 | 0.80 | 外套 |
| soldier_pants | 0 | 0.90 | 裤子 |
| soldier_upBody1 / soldier_upBody2 | 0 | 0.70 | 混合皮肤与服装的贴图区域 |
| Water | 0 | 0.08 | 静态水面近似 |
| IcePanel | 0 | 0.12 | 冰面近似 |

砖块和地砖已接入原作者配套的 `bricks2_nmap` / `tile_nmap` 法线图，以及导出脚本根据沟槽分布制作的打包粗糙度图；G 通道乘表中粗糙度，B 通道为 1 并乘材质金属度。`normal_scale` 分别为 0.7 / 0.5。重新导出需要源 `Assets/Textures` 中的两张对应 DDS。来源见 [SOURCE.md](../Assets/RendererLab/SOURCE.md)。其他资产继续用材质系数表达表面差异。旧漫反射纹理仍包含部分烘焙明暗与高光，粗糙度图也是外观近似，不能视为测量资产。

默认场景已开启 IBL、方向光级联阴影、两个点光和一个聚光。水和冰仍是不透明表面，具有天空环境反射，但没有透射、折射、水波或场景内物体反射；这些效果需要独立渲染功能。皮肤尚无次表面散射。

## 环境光、阴影与局部灯

环境图在 Renderer 初始化时通过一次 GPU Compute 预计算：16×16 漫反射 Cube 存 irradiance/π，64×64、7 级 Mip 的镜面 Cube 使用 GGX 重要性采样，64×64 BRDF LUT 积分与直接光一致的 Smith 可见性。漫反射和镜面预过滤各用 128 个样本，LUT 用 256 个样本。运行时按法线与反射方向采样，粗糙度控制镜面 LOD，Split-sum 组合 LUT 与 F0；资源由 SceneAssets 持有，不建立额外缓存系统。环境旋转同时作用于天空和 IBL，环境强度单独控制照明；天空原有 intensity/exposure 作为辐亮度倍率同时进入环境照明。

方向光使用 4×1024² 深度数组，按相机视深选择级联。分割采用 60% 对数与 40% 均匀混合，包围球与光空间 texel 对齐稳定投影；级联末端 10% 混合，3×3 PCF、光栅深度偏移及小法线偏移控制锯齿和自阴影。阴影与主绘制共用纹理 Alpha、基础色 Alpha、UV 和 cutoff。阴影绘制不复用相机剔除结果，避免屏外投影物体被漏掉。当前固定给接收体前方保留 100 个场景单位的投影深度余量，超大场景需调整；仅透视主相机启用 CSM，Cluster 调试不参与场景阴影。

局部灯存为场景中的世界空间值，通过 RenderFrame 快照提交，最多 64 盏。点光采用平方反比与平滑范围截断；聚光额外使用内外锥平滑衰减。当前逐像素遍历灯列表，点光/聚光尚不投射阴影；在场景规模确有需要时再加入灯光分块和局部阴影。Scene 面板的 Lighting 可编辑、添加、删除和保存这些设置，编辑时重置 TAA 历史。

Scene 面板的 GPU timings 显示剔除、方向光阴影、场景/天空、TAA/显示及编辑器合成的 GPU 毫秒数。查询按帧槽持有，录制提交后标记有效，槽位 Fence 完成再读回；不为计时增加每帧 CPU 等待。环境预计算是加载工作，不计入正常帧的这些区间。

## 帧内颜色流程

```text
颜色纹理 sRGB SRV 解码 / 数据纹理线性采样
    → 方向光 CSM 深度（与主绘制共用透明裁剪）
    → PBR 方向光/局部灯 + IBL + 可选环境填充 + 自发光
    → 与线性 HDR 天空合成到 RGBA16F
    → HDR TAA（历史保留大于 1 的亮度）
    → 统一曝光和色调映射
    → 精确分段 sRGB 编码
    → 编辑器 Gizmo / UI 显示合成
```

场景曝光同时影响物体和天空，不影响最终叠加的编辑器 UI/Gizmo。法线调试视图跳过曝光和色调映射，仍遵循输出编码。基础色常量、自发光和光照强度不应提前做显示 Gamma 转换。

## 查看与验证

打开 `Assets/Scenes/PBRMaterialLab.mscene`，或运行：

```powershell
.\out\visual-studio\bin\Release\MEngineEditor.exe --scene .\Assets\Scenes\PBRMaterialLab.mscene
```

上排为粗糙度 0.15/0.45/0.85 的非金属，下排为同样粗糙度的金属，底部为 HDR 自发光球。材质球已开启 IBL，可以对比环境反射随粗糙度变化。

PBRMaterialTests / PBRHardwareTests 验证 sRGB 解码、线性空间过滤、材质系数、金属/非金属 GGX 响应、线性参数纹理、法线贴图、Alpha Mask、自发光与 HDR TAA。资产测试验证源文件/Cook 一致及旧版材质读取。

LightingTests / LightingHardwareTests 验证恒定环境能量、环境旋转和粗糙度预过滤、点光范围、聚光锥、多光源叠加、级联阴影与透明裁剪投影、取消录制后的子资源状态和 GPU 计时。资产测试还验证 sRGB/Alpha Mip、法线归一化和非 2 次幂边缘。

SceneFileRenderingTests / SceneFileHardwareTests 在实际默认场景连续绘制 48 帧，使用最后 16 帧的牙齿区域亮度变化约束高光闪烁回归，并分别在 WARP 和硬件设备执行。

参考：[glTF 材质与颜色定义](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#materials)、[Filament PBR 模型](https://google.github.io/filament/Filament.html)。
