# RendererLab 场景资产来源

模型、纹理来自本地 RendererLab/Assets，原始来源为 Frank Luna 的 d3d12book 配套资产。
原作者 Frank Luna；沿用原资产使用条件，未声明额外分发授权。
场景尺寸、位置和材质颜色参考 RendererLab/App/DefaultScene.cpp。

导出脚本 Tools/ExportRendererLabScene.py 将 skull.txt 转为 glTF、DDS 转为 PNG，并把 soldier.m3d 的 Take1 在 0 秒和 0.5 秒的姿态烘焙为普通静态网格。
立柱、球体、网格地面和曲面导出为普通三角形网格；树木使用固定朝向的透明裁剪面片。
当前保持原场景布局和几何内容，材质已按砖石、地砖、金属、织物和皮肤配置金属度与粗糙度。这些参数为外观初值，不是实测值，详见 docs/Materials.md。
砖块与地砖法线来自 https://github.com/d3dcoder/d3d12book/tree/master/Textures 中对应的 bricks2_nmap.dds 和 tile_nmap.dds；打包粗糙度是导出脚本根据沟槽分布制作的外观近似，非实测。
水面为静态几何，水和冰使用低粗糙度不透明非金属近似；不包含平面反射、SSAO、水波、透射或实时骨骼动画。
天空使用 MEngine 当前天空盒，并参与 IBL；方向光投射级联阴影，场景中另有点光和聚光。
