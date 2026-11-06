# 早期分簇算法笔记

原位于 `Source/Renderer/Nanite/ClusterBuilder/1`。本文保留早期思路；后续设计与实现差异见 [Nanite 核心系统计划](nanite-core-system-plan-and-review.md)。

1.将所有边假如一个哈希表，一条有向边唯一代表一个tri，一对相反的边代表一对相邻三角
2.并查集用来区分不连通的mesh集合
3.使用莫顿码对三角中心->mesh包围盒空间位置进行排序，并且加入MaterialIndexes以避免不同材质的mesh合并
4.加权三角图构造：真实共享边加权260 空间位置相近加权1
5.构造Range以及相应的cluster
6.计算cluster的边：检查每条边的三角形是否属于当前range 若否，则为外部边
7.标记为叶子 Cluster，对于一开始的原始mesh，使用-1标记为原始mesh
8.记录 Mesh 的叶子入口
