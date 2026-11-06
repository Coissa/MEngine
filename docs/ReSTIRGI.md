# 简化 ReSTIR GI

本实现独立编写，不包含或链接 RTXDI 的头文件、Shader 或运行库。目标是逐版学习：一跳漫反射 → 时间 reservoir → 上一帧邻居复用。每版的 GI 计算和 HDR 合成均为一次 Compute Dispatch；光栅化、BLAS/TLAS 构建与 TAA 在外部执行。

## 已保存版本

| 目录（位于 out/gi-versions） | 内容 |
|---|---|
| 00-baseline | 开发前版本，源码提交 ad8bb1a |
| 01-one-bounce | 每像素一个余弦半球候选；次级表面直接光、自发光；未命中时采样天空 |
| 02-temporal | 两份 reservoir 交替读写，重投影复用上一帧同一表面的样本 |
| 03-spatiotemporal | 时间复用加四个上一帧空间邻居；编辑器可切换全部三种模式 |

每个目录包含独立的 MEngineEditor.exe、Shaders、Assets、Launch.ps1 和 VERSION.txt。用 Launch.ps1 启动，VERSION.txt 记录对应提交。二进制在 out 中保留，不加入 Git。源码可通过 git switch --detach <commit> 回看；有未提交改动时先保存自己的工作。

重新构建和留档：

```powershell
cmake -S . -B out/restir-build -G "Visual Studio 18 2026" -A x64 -T host=x64 -DBUILD_TESTING=ON -DCMAKE_TRY_COMPILE_CONFIGURATION=Release
cmake --build out/restir-build --config Release --target MEngineEditor ReSTIRGITests --parallel 4
ctest --test-dir out/restir-build -C Release -R ReSTIRGITests --output-on-failure
# 在该版本提交后执行，确保 VERSION.txt 对应实际构建的源码。
powershell -ExecutionPolicy Bypass -File Tools/SaveGIVersion.ps1 -Version 01-one-bounce
```

## 第一版

ScenePass 增加世界空间着色法线和漫反射反照率两个 MRT；位置从抖动深度和相机矩阵重建。独立 GIPSMain 入口使原有不启用 GI 的渲染测试保持原路径。Compute 使用 DXR 1.1 RayQuery 追踪一个余弦加权方向，命中后计算次级表面 Lambert 直接光和自发光。太阳及抽样的一个局部灯使用真实几何遮挡；局部灯结果乘灯数量，补偿均匀选灯 PDF。未命中时读取天空。

启用时替换环境常量与漫反射 IBL，保留镜面 IBL、主表面直接光和自发光。关闭时恢复原光照。编辑器 Scene 面板提供 Diffuse GI 开关；没有 DXR 1.1 的设备保留原前向路径。RenderFrame 的 enable_gi 默认为 false，由编辑器显式开启。

BLAS 随场景创建；TLAS、实例上传及常量缓冲按三个帧槽持有，复用前由现有 Fence 等待。TLAS 使用完整场景实例，不受相机视锥剔除影响。

目前次级表面仅使用材质基础颜色、金属度、自发光常量，不采样纹理和法线贴图；Alpha Mask 物体不进入光追加速结构。支持静态网格及实例变换，不支持骨骼变形、Cluster 调试几何、多次反弹和镜面 GI。反射采用固定 0.96 漫反射系数。没有独立 GI 降噪器，TAA 可帮助降低噪声，但关闭 TAA 时单样本噪声明显。

次级表面目前评估太阳、局部灯和自发光，尚未继续采样次级表面接受的天空照明；天空只在主间接光线未命中时采样。

## 第二版：时间复用

每像素 reservoir 保存次级位置/法线、辐射亮度、归一化权重 W、候选数 M、样本年龄及来源表面位置/法线/反照率，共 96 字节。天空样本单独标识并保存方向。使用当前表面 Lambert 贡献的亮度作为 target；新样本权重为 target/pdf，历史合并权重为 target_current × W_history × M_history × Jacobian，结束后用 S/(M × selectedTarget) 归一化。有限次级表面按立体角变换计算 Jacobian；天空方向的 Jacobian 为 1。

重投影使用运动矢量并补偿前后两帧 raster jitter。表面世界位置、法线与反照率不匹配时拒绝历史；限制来源 M 为 16、样本年龄为 20 帧。归一化与选中样本在当前表面的可见性测试均在同一次 dispatch 中完成。当前实现仅查询最终样本可见性，并拒绝极端 Jacobian，属于有偏简化版，不宣称论文的无偏校正。

GI 历史独立于 TAA，关掉 TAA 仍会复用 reservoir。历史索引只在 Renderer::CommitFrame 后推进。关闭 GI、Resize、显式 ResetHistory、相机 ID/投影/场景修订变化、实例或光照快照变化会清空历史。相机位置变化通过重投影处理；移动物体时保守重置全屏 GI，尚未实现动态路径重连。

## 第三版：融合历史邻居复用

时间样本之外，在重投影位置周围 3–8 像素半径选择四个上一帧邻居，使用旋转采样方向分散邻居。按世界空间法线、接收表面的平面距离、空间半径和反照率匹配；每个来源仍计算目标贡献及 Jacobian。只读取 PrevReservoir，最终写入 CurrReservoir，不读取同一 dispatch 内其他线程的输出，也不使用全局同步技巧。反遮挡区域先依赖本帧新样本，邻居复用会晚一帧开始。

编辑器 Scene 面板的 ReSTIR GI 开关下可选择 One bounce、Temporal、Temporal + history neighbors；切换模式会使 GI 与 TAA 历史失效。观察采样噪声时建议暂时关闭 TAA。GPU timings 的 GI + TAA + display 包含 TLAS 构建、GI 与后处理，尚未拆分单独的 GI 计时。

源码回退点：开发前 ad8bb1a；第一版 c918d0f；第二版 38910fe；第三版完整提交号记录在 03-spatiotemporal/VERSION.txt。

## 验证

ReSTIRGITests 在硬件上以红色自发光侧墙和无直接光的白墙验证间接照明，保存 gi-disabled.ppm、gi-enabled.ppm，并检查取消录制、历史重置、Resize 和 GI 开关。现有 LightingTests、LightingHardwareTests 和 TAATests 用于验证前向光照与 TAA。编辑器通过 --smoke 生成 editor-smoke.ppm 并检查 D3D12 调试层错误。

第三版额外保存三个模式的独立画面，验证第二帧空间复用的覆盖率高于纯时间复用；恒定天空下验证归一化不会改变亮度、相机移动后亮度稳定、关闭环境照明不会留下历史辐射亮度。这些图像关闭 TAA，比较的主要是采样覆盖与噪声，不是最终降噪质量。源实现没有做完整偏差校正，不应把该场景的亮度或覆盖率当成任意场景的无偏性或性能证明。

最终 Release 构建通过；12 项受影响的渲染验证通过，覆盖 GI、光照、材质、运动矢量、TAA、多渲染目标以及硬件/WARP 编辑器短运行。独立运行包中的 validation.txt 保存对应测试结果；最终版本还保存三个模式的无 TAA 对比图。

参考：[RTXDI GI 文档](https://github.com/NVIDIA-RTX/RTXDI/blob/main/Doc/RestirGI.md)、[DXR 官方规范](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html)。
