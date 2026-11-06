# 构建与验证

需要 Visual Studio 2026 的 C++ 桌面开发工作负载、Windows SDK（含 DXC）和 CMake 4.4+。

```powershell
powershell -ExecutionPolicy Bypass -File .\GenerateSolution.ps1
cmake --build --preset windows-vs2026-release
.\out\visual-studio\bin\Release\MEngineEditor.exe
```

也可以打开根目录 `MEngine.slnx`，选择 `Release | x64`，启动 `MEngineEditor`。默认只有 `Engine` 静态库、`MEngineEditor` 两个编译目标；`ALL_BUILD`、`ZERO_CHECK` 是 CMake 辅助项目。

`Source/CMakeLists.txt` 管理引擎源码和资源打包。`cmake/Shaders.cmake` 显式登记 HLSL 文件、入口和 Profile，增量生成 exe 旁的 `Shaders/*.cso`；运行时不需要 HLSL 源文件。新增 Shader 入口后更新登记并重新生成解决方案。Shader 使用 DXC、HLSL 2021 和 SM6.6 编译，仍以 `.cso` 文件供运行时加载；可通过 `MENGINE_DXC` 指定编译器路径。主要运行硬件为 RTX 5070；WARP 仅用于显式软件测试，不作为硬件初始化失败后的自动回退。

解决方案中的所有 `.cso` 统一显示在 `Engine → Shaders → Bytecode` 筛选器中，包含 Debug、Release 两种配置的输出。

项目自有 C++/HLSL 文件遵循根目录 `.clang-format`，统一使用四空格缩进和换行大括号；其他文本文件的基础格式见 `.editorconfig`。第三方代码保留原始格式。

输出位于 `out/visual-studio/bin/<配置>/`。Debug 预设为 `windows-vs2026-debug`；本机曾出现 MSVC 的 MSPDB140.DLL 版本冲突，遇到该链接错误需要修复本机工具链，不应通过关闭项目功能绕过。

## 可选测试

```powershell
cmake -S . -B out/tests -G "Visual Studio 18 2026" -A x64 -T host=x64 -DBUILD_TESTING=ON -DCMAKE_TRY_COMPILE_CONFIGURATION=Release
cmake --build out/tests --config Release --parallel 1
ctest --test-dir out/tests -C Release --output-on-failure
```

RHIHardwareTargetsTests 和 EditorHardwareSmoke 在默认高性能硬件适配器上验证实际 Shader 与编辑器渲染，标记为 hardware。其他测试覆盖编辑器、PIE、组件、数学、资产/Cook、场景版本迁移、父子层级，以及 WARP 上的录制取消、三个帧槽、两个后台缓冲、Resize、GPU 剔除、多颜色目标和深度采样。MotionVectorTests 验证物体/相机/天空速度、稳定 ID、提交后推进历史、历史重置以及 Gizmo 单目标绘制。

TAATests 验证历史累积/裁剪、越界拒绝、颜色转换、取消录制、开关/相机/Resize 重置以及 Gizmo 历史隔离，并以 4×4 超采样参考画面比较斜边误差。测试在 exe 输出目录生成 taa-off.ppm、taa-on.ppm、taa-reference.ppm。

```powershell
# 使用 WARP 短运行，输出 editor-smoke.ppm，检查可用的 D3D12 调试层错误后退出。
.\out\visual-studio\bin\Release\MEngineEditor.exe --smoke --warp

# 默认启动 Assets/MEngine.project 中的场景；也可选择文件或导入 glTF 验证 Cluster 调试。
.\out\visual-studio\bin\Release\MEngineEditor.exe --scene .\Assets\Scenes\TwoCubes.mscene
.\out\visual-studio\bin\Release\MEngineEditor.exe --cluster-mesh model.gltf
```

`--shader-dir` 可指定包含构建字节码的 Shaders 目录，其父目录应同时包含 Assets。

反射生成器继续通过原有 `MENGINE_BUILD_REFLECTION_GENERATOR_CLANG`、`MENGINE_ENABLE_REFLECTION_CODEGEN` 等选项启用；使用 LLVM/Clang 22.1，代码生成需使用能输出 `compile_commands.json` 的 Ninja 预设。默认构建直接使用已提交的运行时反射代码。生成器读取 Runtime.Reflection.json 并自动生成组件注册和读写实现，新增反射头文件需要加入此 manifest。

在 Visual Studio x64 开发者命令环境中运行：

```powershell
cmake -S . -B out/reflection-codegen -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DMENGINE_ENABLE_REFLECTION_CODEGEN=ON -DMENGINE_BUILD_REFLECTION_GENERATOR_TESTS=ON -DLLVM_DIR=D:/DevTools/LLVM/22.1.8/lib/cmake/llvm -DClang_DIR=D:/DevTools/LLVM/22.1.8/lib/cmake/clang
cmake --build out/reflection-codegen --target MEngineGenerateReflection MEngineReflectionGeneratorTests
.\out\reflection-codegen\MEngineReflectionGeneratorTests.exe
```

LLVM 路径按实际安装位置调整。无需编译整个编辑器即可运行 MEngineGenerateReflection；完整构建会自动先更新生成代码。使用和扩展契约见 [Reflection.md](docs/Reflection.md)。

场景和资源格式见 [SceneFiles.md](docs/SceneFiles.md)。SceneFileRenderingTests 覆盖文件驱动资源、场景切换、保存重载、PIE 旋转隔离、曝光与天空隔离、独立 DebugPass，以及资源加载失败后保留原场景。
