# 简化 ReSTIR GI

本实现独立编写，不包含或链接 RTXDI 的头文件、Shader 或运行库。目标是逐版学习：一跳漫反射 → 时间 reservoir → 上一帧邻居复用。每版的 GI 计算和 HDR 合成均为一次 Compute Dispatch；光栅化、BLAS/TLAS 构建与 TAA 在外部执行。

## 已保存版本

| 目录（位于 out/gi-versions） | 内容 |
|---|---|
| 00-baseline | 开发前版本，源码提交 ad8bb1a |
| 01-one-bounce | 每像素一个余弦半球候选；次级表面直接光、自发光；未命中时采样天空 |

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

## 验证

ReSTIRGITests 在硬件上以红色自发光侧墙和无直接光的白墙验证间接照明，保存 gi-disabled.ppm、gi-enabled.ppm，并检查取消录制、历史重置、Resize 和 GI 开关。现有 LightingTests、LightingHardwareTests 和 TAATests 用于验证前向光照与 TAA。编辑器通过 --smoke 生成 editor-smoke.ppm 并检查 D3D12 调试层错误。

参考：[RTXDI GI 文档](https://github.com/NVIDIA-RTX/RTXDI/blob/main/Doc/RestirGI.md)、[DXR 官方规范](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html)。
