# MEngine Nanite 核心系统：源码流程、代码复查与实施顺序

日期：2026-10-02。本文依据 E 盘 UE5.8.1 和 D 盘 MEngine 当前文件重新阅读整理；状态描述是本次检查时的快照。

## 1. 目标与当前结论

目标是在 MEngine 中展示静态三角网格的 Nanite 核心能力：离线分簇与分层简化、运行时按屏幕误差选簇、Cluster 级可见性剔除、GPU 生成间接绘制参数，以及编辑器中的 Cluster/LOD 染色和数量变化。

采用全量常驻几何、Compute Shader 选择和现有硬件光栅绘制。第一版不要求 UE 的页格式、压缩、磁盘流送、软件光栅、Visibility Buffer 或完整材质系统。HZB 遮挡剔除在基本闭环之后按需求加入。

当前离线几何主流程已有，但离线阶段尚未收尾。主要缺口是遍历关系没有可编译、可调用的完成版本，缺少嵌套的 LOD 包围体，以及没有导出可以上传 GPU 的扁平描述和几何数组。下一阶段应先完成这些，再实现 GPU 选簇。

本次只新增本文档，没有修改引擎源码。阅读范围包括 NaniteBuilder、ClusterDAG、Cluster、MeshSimplifier、Encode 主流程与层级编码、运行时剔除调度和 Shader 遍历/LOD 判断；没有逐项复刻曲线、体素、骨骼、阴影、程序集装和流送分支。

## 2. UE 的完整主流程与本项目的取舍

### 2.1 UE 离线构建

```text
FBuilderModule::Build
  └─ BuildIntermediateResources
       ├─ 读取顶点、三角索引、材质与构建设置
       ├─ FClusterDAG::AddMesh：拓扑、连通岛、图划分、叶 Cluster
       └─ FClusterDAG::ReduceMesh
            └─ 按层循环：邻接 → 分组 → ReduceGroup
                 ├─ 合并 Group.Children 的几何
                 ├─ SimplifyTriangles：带属性的 Quadric 简化
                 ├─ SplitCluster：拆成父 Cluster
                 └─ 写入误差、LODBounds、GroupIndex、GeneratingGroupIndex
  ├─ 可选 Trim / Fallback / RayTracing Fallback
  └─ Encode
       ├─ 材质范围、Cluster 约束与顶点复用
       ├─ 量化和编码信息
       ├─ Cluster 分配到 Pages / GroupParts
       ├─ BuildHierarchies：运行时空间层级
       └─ 页依赖、Fixup 和 WritePages
```

| UE 源码入口 | 阅读要点 | MEngine 对应/取舍 |
|---|---|---|
| [NaniteBuilder.cpp:899](<E:/UnrealEngine/UE5.8.1/Engine/Source/Developer/NaniteBuilder/Private/NaniteBuilder.cpp:899>)，`FBuilderModule::Build` | 外层编排：中间构建、可选 fallback、最终 Encode | 保留“构建几何 → 整理运行时数据”的职责划分 |
| [NaniteBuilder.cpp:575](<E:/UnrealEngine/UE5.8.1/Engine/Source/Developer/NaniteBuilder/Private/NaniteBuilder.cpp:575>)，`BuildIntermediateResources` | 输入转换，逐 Mesh 调用 AddMesh/ReduceMesh | `StaticMeshAsset` 已包含必要输入；按带几何的 node 构建 |
| [ClusterDAG.cpp:18](<E:/UnrealEngine/UE5.8.1/Engine/Source/Developer/NaniteBuilder/Private/ClusterDAG.cpp:18>)，`AddMesh` | 按位置匹配有向半边、并查集、局部性链接、图划分、生成叶簇 | 已有 `BuildTriangleTopology`、`PartitionTriangles`、`BuildLeafClusters` |
| [ClusterDAG.cpp:593](<E:/UnrealEngine/UE5.8.1/Engine/Source/Developer/NaniteBuilder/Private/ClusterDAG.cpp:593>)，`FindAdjacentClusters`；[750](<E:/UnrealEngine/UE5.8.1/Engine/Source/Developer/NaniteBuilder/Private/ClusterDAG.cpp:750>)，`GroupTriangleClusters` | 用外部边建立簇邻接，按图和位置分组；对同生成 Group 的兄弟簇增加关联权重 | 已有 `BuildClusterGraph`、`GroupClusters`；贪心分组可作为第一版 |
| [ClusterDAG.cpp:1067](<E:/UnrealEngine/UE5.8.1/Engine/Source/Developer/NaniteBuilder/Private/ClusterDAG.cpp:1067>)，`ReduceMesh` | 循环生成更粗层，最终建立根 Group | 已有同名流程；本项目允许最顶层多个根 Cluster |
| [ClusterDAG.cpp:1375](<E:/UnrealEngine/UE5.8.1/Engine/Source/Developer/NaniteBuilder/Private/ClusterDAG.cpp:1375>)，`ReduceGroup` | 合并、简化、拆分，累计误差和嵌套包围体；父簇共享 LOD 数据 | 已有几何步骤；需要补 LOD 包围体和索引关系 |
| [Cluster.cpp:911](<E:/UnrealEngine/UE5.8.1/Engine/Source/Developer/NaniteBuilder/Private/Cluster.cpp:911>)，`SimplifyTriangles`；[MeshSimplify.cpp:996](<E:/UnrealEngine/UE5.8.1/Engine/Source/Developer/MeshSimplifier/Private/MeshSimplify.cpp:996>)，`Simplify` | 带属性 Quadric、边界位置锁定、折叠误差、几何尺度转换 | 当前简单几何 QEM 可以先保留；不能声称等同 UE 简化质量 |
| [ClusterDAG.cpp:1604](<E:/UnrealEngine/UE5.8.1/Engine/Source/Developer/NaniteBuilder/Private/ClusterDAG.cpp:1604>)，`FindCut` | 构建期按三角形预算/误差找 cut，用于 Trim 和粗表示 | 不是运行时选簇入口；只作离线调试参考，非必需新功能 |
| [Encode/NaniteEncode.cpp:1411](<E:/UnrealEngine/UE5.8.1/Engine/Source/Developer/NaniteBuilder/Private/Encode/NaniteEncode.cpp:1411>)，`Encode` | 将构建中间数据转换为 GPU 资源 | 本项目只需未压缩的扁平数组，跳过 Pages/Fixup/位编码 |
| [Encode/NaniteEncodeHierarchy.cpp:679](<E:/UnrealEngine/UE5.8.1/Engine/Source/Developer/NaniteBuilder/Private/Encode/NaniteEncodeHierarchy.cpp:679>)，`BuildHierarchy`；[826](<E:/UnrealEngine/UE5.8.1/Engine/Source/Developer/NaniteBuilder/Private/Encode/NaniteEncodeHierarchy.cpp:826>)，`BuildHierarchies` | 按 LOD 组织空间层级，再组合各层根；叶引用 GroupPart | 是加速可见性和候选筛选的层级，不是简化 DAG 本身；第一版可以暂缓 |

### 2.2 UE 运行时

```text
实例/视图数据
  → GPU Instance Culling
  → GPU Hierarchy Node 遍历与 LOD 范围筛选
  → GPU Cluster LOD 判断与视锥/HZB 剔除
  → Visible Clusters
  → Raster Binning
  → HW/SW Rasterization
  → 后续材质着色与调试显示
```

| UE 源码入口 | 功能 |
|---|---|
| [NaniteCullRaster.cpp:4500](<E:/UnrealEngine/UE5.8.1/Engine/Source/Runtime/Renderer/Private/Nanite/NaniteCullRaster.cpp:4500>)，`FRenderer::AddPass_NodeAndClusterCull` | 绑定 Hierarchy/Cluster/候选队列/输出等资源，调度 Compute |
| [NaniteClusterCulling.usf:969](<E:/UnrealEngine/UE5.8.1/Engine/Shaders/Private/Nanite/NaniteClusterCulling.usf:969>)，`NodeAndClusterCull` | GPU 节点与 Cluster 剔除入口 |
| [NaniteHierarchyTraversal.ush:251](<E:/UnrealEngine/UE5.8.1/Engine/Shaders/Private/Nanite/NaniteHierarchyTraversal.ush:251>)，`PersistentNodeAndClusterCull` | 持久线程消费节点与 Cluster 队列；同文件也有分阶段 NodeCull/ClusterCull |
| [NaniteClusterCulling.usf:258](<E:/UnrealEngine/UE5.8.1/Engine/Shaders/Private/Nanite/NaniteClusterCulling.usf:258>)，`ShouldVisitChildInternal` | 层级节点依据 MaxParentLODError 等值决定是否访问候选 |
| [NaniteClusterCulling.usf:287](<E:/UnrealEngine/UE5.8.1/Engine/Shaders/Private/Nanite/NaniteClusterCulling.usf:287>)，`SmallEnoughToDraw` | 判断 Cluster 自身误差是否满足屏幕要求 |
| [NaniteCullingCommon.ush:112](<E:/UnrealEngine/UE5.8.1/Engine/Shaders/Private/Nanite/NaniteCullingCommon.ush:112>)，`GetProjectedEdgeScales` | 结合 LOD 球、视图、实例缩放、近裁剪面计算投影尺度 |
| [NaniteClusterCulling.usf:722](<E:/UnrealEngine/UE5.8.1/Engine/Shaders/Private/Nanite/NaniteClusterCulling.usf:722>)，`ProcessCluster` | Cluster 级 LOD、视锥/HZB 判断和可见输出 |
| [NaniteCullRaster.cpp:6731](<E:/UnrealEngine/UE5.8.1/Engine/Source/Runtime/Renderer/Private/Nanite/NaniteCullRaster.cpp:6731>)，`DrawGeometry` | 主剔除/光栅 pass；启用两阶段遮挡时重建 HZB，再执行 post pass |

第一版借鉴误差、包围体和选簇语义即可，不需要实现 UE 的持久线程队列、流送请求、光栅分箱和软件光栅。Compute Shader 完全可以先并行检查常驻的全部 Cluster，确认效果后再按性能需求增加空间层级。

## 3. 必须明确的数据关系

### 3.1 Group 有两个方向

```text
Level 0：C0 C1 C2 C3
             ↓ 归入
            Group G0
             ↓ 合并、简化、拆分
Level 1：   P0 P1
             ↓ 再分组
          更高层 Group
```

一个 Group 包含多个 children，也能生成多个 parents；每个 Cluster 只由一个 Group 生成，并且在下一轮只归入一个所属 Group。同生成 Group 的父 Cluster 共用细层 children，没有各自独立的子树。

| 字段/关系 | 意义 | 边界情况 |
|---|---|---|
| `generating_group_index` | 生成此 Cluster 的较细层 Group；用于找回细层 children | 叶 Cluster 为无效 ID |
| `group_index` / owner Group | 此 Cluster 被划入的 Group；该组生成更粗表示 | 最顶层 Cluster 没有更粗 owner，可使用根标记 |
| `Group.children` | 被该 Group 合并的细层 Cluster | 现有实现是层内索引 |
| `Group.parent_begin/count` | 此 Group 生成的下一层 Cluster 范围 | 现有实现是下一层的局部范围 |

UE 在 `ReduceGroup` 中为 children 写 `GroupIndex`，为 parents 写 `GeneratingGroupIndex`。这两者不是一个字段，也不需要 `vector<group_id>`。

先前“只加 generating_group_index 就够了”的建议仅适用于从粗层找细层。若采用平铺的 GPU LOD 区间判定，还必须能查到所属 Group 的更粗误差和 LOD 包围体。可以在最终整理时推导 owner 关系，不必提前在每个中间结构重复保存一份。

### 3.2 当前索引约定正确，但导出必须统一

对于层 `L` 中的 Group：

```cpp
global_group_id = dag.levels[L].group_begin + local_group_id;
global_child_id = dag.levels[L].cluster_begin + group.children[i];
global_parent_id = dag.levels[L + 1].cluster_begin + group.parent_begin + i;
```

`generating_group_index` 应存 DAG 内全局 Group ID。构建结构保留层内 children/parent 范围即可；GPU 导出时一次转换为资源内全局 ID。多个节点的 DAG 汇总到一套数组后，再加各 DAG 的 Cluster/Group 基址。

`levels[0]` 是最细叶层，`levels.back()` 是最粗层。最粗层可以包含多个根 Cluster。无需为访问最顶层额外复制一个 root 列表，也无需复制叶 Cluster。

### 3.3 几何包围体与 LOD 包围体

几何 AABB 表示此 Cluster 实際要画的顶点范围，用于视锥剔除。LOD 包围体表示此次细节替换涉及的空间范围，用于误差投影。不能拿拆分后每个父 Cluster 各自的几何 AABB 代替共享 LOD 范围，否则同组父 Cluster 可能作出不一致的细化判断。

叶 Cluster 的 LOD 包围体从自身几何初始化。对每个 Group，合并子 Cluster 的 LOD 包围体，得到嵌套的 Group LOD 包围体；该组产生的所有父 Cluster 共享此值。不要仅从简化后的粗网格重新计算 LOD 包围体，因为那可能丢掉细层空间范围。

UE 用球表示 `LODBounds`，并在 `ReduceGroup` 中合并子 LOD 球。MEngine 可用简单的保守包围球；用中心加到子球中心的距离与子球半径求最大覆盖半径即可，不必做最小包围球优化。每层误差与包围体必须使用一致的模型空间、实例缩放和投影规则。

### 3.4 误差和运行时 cut

本地 `SimplifyMergedCluster` 已计算边折叠的几何 QEM 代价，返回实际折叠最大代价的平方根。`BuildParentClusters` 已执行：

```text
Group.parent_lod_error = max(各 child.lod_error, 本次 simplify_error)
每个生成的 parent.lod_error = Group.parent_lod_error
```

UE 同样通过 max 强制误差沿细层到粗层单调。该值是近似 LOD 指标，不是相对原始表面的严格最大距离，也不是已经算好的像素误差。仅几何 QEM 对共面简化可能返回零，且没有法线/UV 外观权重；第一版需要通过显示效果校准阈值。

一个 Cluster 要被选中，需要同时满足两侧条件：

```text
自身的生成误差投影 <= 像素阈值      // 当前表示已经足够细
所属 Group 的更粗误差投影 > 阈值   // 更粗表示还不够细
```

叶 Cluster 自身误差为零；根 Cluster 没有更粗表示，第二个条件直接成立。比较相等时固定一侧包含等号，另一侧排除等号，避免阈值处重复或缺面。

这里两次投影分别使用自身的生成 Group LOD 范围、所属 Group LOD 范围。整组父 Cluster 共享的误差和 LOD 范围，使整组细化判定一致；嵌套范围和单调误差使粗细条件协调。UE 将较粗门限放在 hierarchy 叶/GroupPart，随后在 `SmallEnoughToDraw` 中测试 Cluster 自身误差。

当前 DAG 每层都会重新分组，上一轮的兄弟父簇可能分属不同的上层 Group，因此不能把它当成普通树，对每个父簇独立 DFS 然后直接输出。第一版平铺 GPU 判定可避开共享 DAG 遍历的去重和部分替换问题。

## 4. 本地代码复查

### 4.1 影响正确性或阻塞下一阶段的事项

| 优先级 | 位置 | 发现与最小处理 |
|---|---|---|
| P1，已确认编译阻塞 | [ClusterDAG.cpp:209](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/ClusterDAG.cpp:209>) | `ClusterTraversalData` 未定义，函数没有返回值；`const ClusterDAG&` 又被修改。改成 `void BuildClusterTraversalData(ClusterDAG& dag)`，声明放在 `Cluster.h`。当前 cpp 只包含拓扑头，看不到 ClusterDAG 定义；可将函数放入已包含 `Cluster.h` 的 `Cluster.cpp`，减少拓扑文件对 Cluster 构建类型的依赖。循环关联父簇的方向和 next.cluster_begin 计算本身正确。 |
| P1，未接入 | [Cluster.cpp:804](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/Cluster.cpp:804>) | `ReduceMesh` 没有调用关联函数，默认生成 Group ID 仍全为无效值。完整追加末层后、返回 DAG 前调用一次。 |
| 离线功能缺口 | [Cluster.h:19](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/Cluster.h:19>)、[Cluster.cpp:633](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/Cluster.cpp:633>) | 误差已有，共享 LOD 包围体没有；GPU 选簇也缺少 owner Group 关系。按第 3 节补齐，不需要改变现有 levels/groups/clusters 的基本布局。 |
| 离线输出缺口 | [Cluster.h:86](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/Cluster.h:86>) | ClusterDAG 含多个 vector，仅是构建中间结果，缺少 GPU 可读取的描述、索引和几何数组。新增一次最终扁平整理即可。 |
| P2，简化候选遗漏 | [ClusterSimplifier.cpp:318](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/ClusterSimplifier.cpp:318>) | 每条边先从两个端点和中点中选最小 QEM 位置，之后仅对该位置做 `CanCollapse`。如果最优位置不合法、其他候选位置合法，当前会跳过整条边，可能过早停止。可在选择位置时同时考虑合法性。这是质量/收敛问题，不代表当前全部模型都出错。 |
| 当前限制，暂可接受 | [Cluster.cpp:780](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/Cluster.cpp:780>) | 父簇数量不下降时停止是合理收敛条件；不能为了唯一根删除三角形或解除必要边界约束。它也可能丢弃“三角形下降但 Cluster 数量相同”的一轮结果，后续仅在模型表现需要时改善。 |
| 编辑器接入缺口 | [EditorMain.cpp:195](<D:/CodeDir/CppProject/MEngine/Engine/Editor/EditorMain.cpp:195>) | 调试入口仍只构建叶层，没有调用完整 ReduceMesh。已有染色只能证明叶分簇，不能证明 DAG/LOD 已显示。离线收尾后补全层/指定层的最小调试入口。 |

单 child Group 在 `BuildParentClusters` 中被直接跳过。当前 `ReduceMesh` 的分组规则对数量大于 1 的层不会生成单 child Group，所以尚不是当前主流程的丢面错误；如果将来改变分组策略，需要保留该 child 作为终止簇或直通父簇，不能静默丢弃。

### 4.2 重复实现和可精简代码

| 位置 | 现状 | 建议 |
|---|---|---|
| [Cluster.cpp:735](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/Cluster.cpp:735>) 与 [772](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/Cluster.cpp:772>) | `BuildLeafClusterLevels(build_parents=true)` 和 `ReduceMesh` 各自编排“图/分组/合并/父簇” | 已有调用只使用 false。让 BuildLeafClusterLevels 只负责叶层，去掉 bool、未使用的 groups/merged_groups/parent_clusters 中间字段；完整层级统一走 ReduceMesh。无需另建通用框架。 |
| [Cluster.cpp:772](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/Cluster.cpp:772>) | 每轮无条件 BuildClusterGraph，但数量 <= 32 时 GroupClusters 直接全归一组，不读 graph | 沿用叶层旧分支的条件：只有超过 Group 最大数量才建图。 |
| [Cluster.cpp:191](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/Cluster.cpp:191>)、[293](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/Cluster.cpp:293>)、[767](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/Cluster.cpp:767>)、[804](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/Cluster.cpp:804>) | 临时 leaf_cluster/Cluster/DAG push_back 深拷贝，ReduceMesh 又复制整个叶层；nowlevel 只是别名 | 对不再使用的临时值使用 move，直接移动 node.clusters；去掉 nowlevel。向 DAG 追加当前层也可移动，前提是完成这一轮所有读取。 |
| [GraphPartitioner.h:23](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/GraphPartitioner.h:23>) | `sorted_to` 被生成，但本地没有读取者；BuildLeafClusters 自己建立 triangle_cluster | 可删除 sorted_to 的存储和末尾生成循环，不影响现有外部边计算。 |
| [ClusterDAG.cpp:176](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/ClusterDAG.cpp:176>) | adjacent 每次 sort/unique；桶按递增 half-edge ID 插入，每个 ID 只出现一次，当前收集也只访问一次 | 在当前串行桶构建方式下排序去重没有作用，可删除。若以后并行插入再重新评估确定性。 |
| [ClusterDAG.h:18](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/ClusterDAG.h:18>) | `triangle_groups`、GroupsMatch 可选路径没有调用者；节点已经拆成本地 mesh | 当前不需要可删除。材质仍保存在 material_indices，不能通过删除/增加此字段改变几何邻接定义。 |
| [DisjointSet.h:16](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Core/Containers/DisjointSet.h:16>) | 当前唯一用户只用构造、Find、Union；另有 ForAll 两个模板、MembersOf、GetSets、UE 别名等未用接口，每次 Find 还做异常式 ValidateIndex | 若只服务当前 Nanite，可精简到初始化、Find、Union 和必要状态；不为 UE API 相似度保留无调用接口。无需另建一个 Nanite 专用并查集。 |
| [MortonCode.cpp:37](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/Nanite/ClusterBuilder/MortonCode.cpp:37>) | 非有限位置统一返回 0，可能掩盖上游坏数据 | 若明确输入为导入后的有限坐标且构建计算不溢出，可省略这项重复防御；坐标量化 clamp 与零 extent 分支仍需保留。 |
| [早期分簇算法笔记](cluster-building-notes.md) | 旧笔记称一条有向边唯一对应三角、材质必然阻止合并、叶子用 -1；与当前实现不一致 | 旧笔记已移入文档目录，保留为历史思路。 |

PositionKey/PositionBits 在拓扑和 Cluster 邻接文件中有重复，清理时可以共享小型位置键帮助函数。但三角拓扑、Cluster 邻接和简化器边表的用途不同，不应为了去重合成复杂的通用图/哈希模块。

`MergedCluster` 的原始副本用于简化和保留合并输入，未必能全部删掉；若移除旧的一层父簇调试路径后，后续不做重新简化尝试，可再把所有权传给 BuildParentClusters，避免当前逐组额外复制。

### 4.3 应保留的代码与已知限制

- 几何位置匹配的边哈希是构建邻接需要的算法数据，不是额外的完整性校验。不同 Cluster 的本地顶点 ID 不能直接拿来比较；UV/法线缝两侧可能有不同顶点 ID，但位置相同。
- 外部边标记、Group 合并时扣除内部连接、简化时边界锁定，是保持 Group 边界一致的必要逻辑。
- `CanCollapse` 中的 link 条件与三角形翻转判断有几何意义，不应当作“多余输入检查”删除。
- 128 三角形、256 顶点限制和拆分逻辑必须保留；uint8 本地索引最多表示 0..255。
- 真实边连通与空间局部性链接是不同关系，不能把近邻当成共享边。非连通小岛可以被装入同簇/同组，但几何不会因此焊接。
- 材质只过滤当前 PartitionTriangles 的跨岛局部性链接；真实拓扑连接和填满分区仍可能使 Cluster 含多个材质。单个 Cluster 不能直接假定只有一个 material_slot。
- 当前简化只对完整 position/normal/UV 相同的顶点做去重，属性缝处保守锁边，法线/UV 没有参与 QEM 打分。普通平滑网格可先展示核心效果；属性缝密集模型可能简化有限，外观质量需实际观察。
- 简化器每次折叠重建边表、候选排序并扫描拓扑，可能成为高模构建瓶颈。先用所需模型测构建时间，出现问题再优化局部更新；不先照搬 UE 的完整 heap/wedge 系统。

### 4.4 本次验证证据

使用本机已有 MSVC 14.51，开启 C++20，以 `/Zs` 对五个 Nanite 构建器 cpp 做语法检查。该模式不生成目标文件。

```text
ClusterDAG.cpp：失败，209 行返回类型处 C2061，随后 C2143/C2447。
Cluster.cpp、ClusterSimplifier.cpp、GraphPartitioner.cpp、MortonCode.cpp：通过语法检查。
```

未执行整引擎编译、链接或新模型运行测试，因此“其他文件语法通过”不代表当前 DAG 运行验证通过。先前的模型验证不能替代这次新增函数后的编译检查。

## 5. 离线阶段的最小完成方案

### 步骤 A：修复并完成 DAG 关联

保留现有 ClusterDAG 的 levels/groups/clusters。修复 `BuildClusterTraversalData(ClusterDAG& dag)` 并在 ReduceMesh 完成后调用一次。

该遍历对每个层间 Group 做两件事：给下一层的 parents 写生成 Group ID；记录本层 children 的所属 Group。owner 可以写 Cluster 字段，也可以在最终导出时推导，二选一，避免保存两份相同映射。

根范围直接来自末层；叶子 generating_group_index 为无效，根 owner 为无效，二者不可混淆。多个根也必须完整保留。

### 步骤 B：在现有构建步骤中补 LOD 包围体

BuildLeafClusters 初始化叶 LOD 球；BuildParentClusters 在合并简化之前合并子 LOD 球，在生成父簇之后统一赋值。Group 保存共享 LOD 球与已有 parent_lod_error。无需再增加一遍重复计算所有几何 AABB 的后处理。

### 步骤 C：导出最小 GPU 数据

建议增加一个最终整理函数，例如：

```cpp
NaniteMeshData PackClusterDAG(std::span<const ClusterDAG> dags);
```

名字是建议，尚未实现。职责仅为平铺几何、转换索引和生成描述，不做再次分簇/再次简化。源 mesh 的节点变换继续由资产/实例数据提供，通过 node_index 对应；不要重复把节点变换烘焙进每个 LOD 的顶点。

| 输出数组 | 最小内容 |
|---|---|
| 顶点数组 | 汇总各层 Cluster 的 position/normal/UV；构建副本与 GPU 上传布局可不同 |
| 索引数组 | 将 uint8 本地索引转换为硬件支持的 uint16；通过每簇 vertex_offset 访问汇总顶点 |
| Cluster 描述数组 | 顶点/索引范围，几何包围体，自身 LOD 误差/球，生成 Group ID、所属 Group ID，node_index；层号用于调试即可 |
| Group 描述数组 | children_begin/count，parent_begin/count，共享 LOD 球与 parent_lod_error |
| children 数组 | 资源内全局 Cluster ID，替代 Group 内部 vector |
| 节点/根范围 | node_index 和最顶层 Cluster 范围，多个根合法 |
| 绘制参数模板 | 每条 draw 的 index_count、first_index、vertex_offset，必要的实例关联 |

第一版使用已有调试材质，可以一簇对应一条 draw。若支持原始多材质，则在导出时按材质生成簇内 draw ranges；不得给混合材质 Cluster 随意取一个材质。不要在构建阶段为了适配绘制重排三角形却忘记同步材质和每条边的元数据。

`external_edges`、三角连通岛和 Morton 工作数组只服务离线构建，无需上传 GPU。不要上传 vector 对象本身；GPU 描述用固定字段，明确 CPU/HLSL 的 stride 和偏移。

### 步骤 D：用最小样例确认离线输出

先检查所有构建器可编译，再使用一个闭合平滑网格和一个开放网格。只验证当前关键行为：叶几何覆盖、Group/Cluster 全局索引、共享 LOD 数据、层间单调性、多根保留和导出 draw 范围。单一误差阈值的离线 cut 可作为辅助验证，不必先实现一套生产用 CPU 运行时选择器。

离线阶段完成标准：静态 mesh 能输出完整且可上传的各层几何与描述；索引能定位上下层关系；误差/包围体足以支持一致的 GPU 判断。磁盘 Cook/Load 在要求跨进程复用构建结果时再做。目前 [CookStaticMesh](<D:/CodeDir/CppProject/MEngine/Engine/Engine/AssetFormats/StaticAssetCookers.cpp:87>) 只保存原始静态网格，尚未保存 Nanite 数据；若每次启动都重新构建，只能称启动时预处理，不能称已完成离线资产缓存。

## 6. 离线完成后的运行时顺序

1. 创建并上传固定 Cluster/Group/顶点/索引缓冲，所有 LOD 常驻。每帧更新相机、像素阈值和实例变换；不重新简化或重新拼接选中几何。
2. Compute 并行检查各 Cluster 的两侧 LOD 条件及几何视锥可见性。共享阈值数据必须使用相同投影函数，必要近裁剪处理保留。
3. 为每簇预留固定 indirect slot；每帧 Shader 明确写 instance_count=0/1。也可以先拷贝全为零的模板，再把可见项置一，但不能留下上一帧的可见状态。
4. Compute 输出完成后执行所需 UAV 屏障和 Indirect 状态转换。共享 VB/IB、同一材质/实例绑定的批次可以一次 ExecuteIndirect 多条命令，不依赖 CPU 读回选簇结果。
5. 编辑器复用现有染色设置，按 Cluster ID/LOD 显示当前选中几何；统计选中 Cluster 与三角形数量。调试统计读回只服务显示，不控制本帧绘制。

相关本地基础已经存在：

| 本地源码 | 已有能力/需要注意 |
|---|---|
| [GpuDrivenScene.cpp:176](<D:/CodeDir/CppProject/MEngine/Engine/Engine/RenderResource/GpuDrivenScene.cpp:176>)，`RecordCull` | Compute 绑定、Dispatch、UAV 屏障、间接参数状态转换；可借鉴组织方式，不把 Cluster 伪装成现有整物体实例 |
| [gpu_scene_culling.hlsl:12](<D:/CodeDir/CppProject/MEngine/Engine/Shaders/gpu_scene_culling.hlsl:12>) | 现有实例视锥剔除 Shader，只有可见性，没有 Cluster LOD 条件 |
| [D3D12Command.cpp:670](<D:/CodeDir/CppProject/MEngine/Engine/Engine/RHI/D3D12/D3D12Command.cpp:670>) | ExecuteIndirect 已接受 max_draw_count，可处理固定长度多条 draw；当前没有 GPU count buffer 入口，第一版无须为压紧可见列表扩展它 |
| [RHIRenderer.cpp:2371](<D:/CodeDir/CppProject/MEngine/Engine/Engine/Renderer/RHIRenderer.cpp:2371>) | 当前每个 submission 单独绑定并执行 1 条 indirect draw；不能直接把所有 Cluster 变成 submission 就声称降低了 CPU 提交开销 |
| [ClusterDebugColorizer.cpp:11](<D:/CodeDir/CppProject/MEngine/Engine/Editor/ClusterDebugColorizer.cpp:11>) | 已有节点变换、Cluster 染色和批量几何整理；可复用颜色和显示约定，固定叶簇 batch 不支持任意动态选簇 |

简单屏幕误差可以从 `error_world * focal_length_pixels / conservative_depth` 开始；它是本项目近似方案，不是 UE GetProjectedEdgeScales 的逐式翻译。保守深度要考虑 LOD 范围和近裁剪面，误差与包围体缩放须一致；第一版可先限定平移、旋转和统一缩放。非统一缩放/剪切需要相应保守变换，不能只变中心。

GPU 平铺扫描复杂度随全部常驻 Cluster 数量增长，这能展示 GPU 细节选择和绘制数量变化，但还不能宣称 UE 同等级的候选访问性能。出现实际瓶颈后再构建简单的空间层级，依据子项最小自身误差/最大父误差聚合数据进行遍历剪枝；再之后考虑 HZB。没有测量需求前不写 Pages、Persistent Queue 或复杂编码。

## 7. 下一次实施的具体范围

第一批：修复编译与关联调用；补 Group/父 Cluster 共享 LOD 包围体和 owner 关系；完成未压缩 GPU 数据整理；验证这份输出。可同时移除旧 build_parents 路径、无用 sorted_to 和明显拷贝，保持既有代码格式。

第二批：在已有 RHI 基础上添加 Cluster 选簇 Compute、固定槽间接参数和共享几何绘制，接编辑器 Cluster/LOD 染色与最小统计。

第三批按测量决定：简化性能/属性质量、空间层级、HZB、构建结果持久化。UE 资产兼容、完整流送、压缩和混合光栅不属于当前必需任务。

实际验收表现是：同一个静态高模近处细、远处粗，视锥外不画；LOD 变化不出现由不一致选簇造成的缺面或重复覆盖；GPU 选择结果直接驱动绘制，编辑器能够观察簇和三角形数量变化。
