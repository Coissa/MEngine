"""Export RendererLab's default content as ordinary MEngine project assets.

Run with --source <RendererLab directory>. Only authoring uses the source tree;
the exported scene has no dependency on RendererLab at runtime.
Models/textures originate from Frank Luna's d3d12book; see the exported SOURCE.md.
"""

import argparse
import bisect
import json
import math
from pathlib import Path
import re
import struct
import uuid
import zlib


def normalize(v):
    length = math.sqrt(sum(x * x for x in v))
    return tuple(x / length for x in v)


def png(path, width, height, pixels):
    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff))
    rows = b"".join(b"\0" + pixels[y * width * 4:(y + 1) * width * 4] for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
                     chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def read_dds(path, layer=0):
    """Decode the source's BC1, BC3 and RGBA8 top mip for offline PNG export."""
    data = path.read_bytes()
    height, width = struct.unpack_from("<II", data, 12)
    fourcc = data[84:88]
    offset = 128
    if fourcc == b"DX10":
        dxgi_format = struct.unpack_from("<I", data, 128)[0]
        if dxgi_format != 28:
            raise ValueError(f"Unsupported export DDS format: {dxgi_format}")
        mips = max(1, struct.unpack_from("<I", data, 28)[0])
        layer_bytes = sum(max(1, width >> m) * max(1, height >> m) * 4 for m in range(mips))
        offset = 148 + layer * layer_bytes
        return width, height, data[offset:offset + width * height * 4]
    if fourcc == b"\0\0\0\0":
        bit_count, red, green, blue, alpha = struct.unpack_from("<5I", data, 88)
        masks = (red, green, blue, alpha)
        shifts = [(mask & -mask).bit_length() - 1 if mask else 0 for mask in masks]
        maxima = [mask >> shift for mask, shift in zip(masks, shifts)]
        channels = bit_count // 8
        pitch = max(width * channels, struct.unpack_from("<I", data, 20)[0])
        pixels = bytearray(width * height * 4)
        for y in range(height):
            for x in range(width):
                begin = offset + y * pitch + x * channels
                value = int.from_bytes(data[begin:begin + channels], "little")
                dest = (y * width + x) * 4
                for c, (mask, shift, maximum) in enumerate(zip(masks, shifts, maxima)):
                    pixels[dest + c] = ((value & mask) >> shift) * 255 // maximum if mask else 255
        return width, height, pixels
    if fourcc not in (b"DXT1", b"DXT5"):
        raise ValueError(f"Unsupported export DDS compression: {fourcc}")
    pixels = bytearray(width * height * 4)
    block_bytes = 8 if fourcc == b"DXT1" else 16
    for by in range((height + 3) // 4):
        for bx in range((width + 3) // 4):
            block = offset + (by * ((width + 3) // 4) + bx) * block_bytes
            alphas = [255] * 16
            if fourcc == b"DXT5":
                a0, a1 = data[block:block + 2]
                table = [a0, a1]
                if a0 > a1:
                    table += [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
                else:
                    table += [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
                bits = int.from_bytes(data[block + 2:block + 8], "little")
                alphas = [table[(bits >> (i * 3)) & 7] for i in range(16)]
                block += 8
            c0, c1, selectors = struct.unpack_from("<HHI", data, block)
            def rgb(value):
                return ((value >> 11) * 255 // 31, ((value >> 5) & 63) * 255 // 63,
                        (value & 31) * 255 // 31, 255)
            colors = [rgb(c0), rgb(c1)]
            if c0 > c1 or fourcc == b"DXT5":
                colors += [tuple((2 * a + b) // 3 for a, b in zip(colors[0], colors[1])),
                           tuple((a + 2 * b) // 3 for a, b in zip(colors[0], colors[1]))]
            else:
                colors += [tuple((a + b) // 2 for a, b in zip(colors[0], colors[1])), (0, 0, 0, 0)]
            for i in range(16):
                x, y = bx * 4 + i % 4, by * 4 + i // 4
                if x < width and y < height:
                    color = colors[(selectors >> (i * 2)) & 3]
                    dest = (y * width + x) * 4
                    pixels[dest:dest + 4] = bytes((*color[:3], min(color[3], alphas[i])))
    return width, height, pixels


def grid(width, depth, rows, columns, height=lambda x, z: 0):
    vertices, indices = [], []
    for i in range(rows):
        for j in range(columns):
            x, z = -width / 2 + width * j / (columns - 1), depth / 2 - depth * i / (rows - 1)
            epsilon = 0.001
            dx = (height(x + epsilon, z) - height(x - epsilon, z)) / (2 * epsilon)
            dz = (height(x, z + epsilon) - height(x, z - epsilon)) / (2 * epsilon)
            vertices.append((x, height(x, z), z, *normalize((-dx, 1, -dz)),
                             j / (columns - 1), i / (rows - 1)))
    for i in range(rows - 1):
        for j in range(columns - 1):
            a = i * columns + j
            indices += [a, a + 1, a + columns, a + columns, a + 1, a + columns + 1]
    return vertices, indices


def box():
    vertices, indices = [], []
    for normal, positions in [
        ((0, 0, -1), [(-1, -1, -1), (-1, 1, -1), (1, 1, -1), (1, -1, -1)]),
        ((0, 0, 1), [(1, -1, 1), (1, 1, 1), (-1, 1, 1), (-1, -1, 1)]),
        ((-1, 0, 0), [(-1, -1, 1), (-1, 1, 1), (-1, 1, -1), (-1, -1, -1)]),
        ((1, 0, 0), [(1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)]),
        ((0, 1, 0), [(-1, 1, -1), (-1, 1, 1), (1, 1, 1), (1, 1, -1)]),
        ((0, -1, 0), [(-1, -1, 1), (-1, -1, -1), (1, -1, -1), (1, -1, 1)])]:
        base = len(vertices)
        for position, uv in zip(positions, [(0, 1), (0, 0), (1, 0), (1, 1)]):
            vertices.append((*[v * 0.75 for v in position], *normal, *uv))
        indices += [base + i for i in (0, 1, 2, 0, 2, 3)]
    return vertices, indices


def sphere(radius=0.65, slices=24, stacks=16):
    vertices, indices = [], []
    for i in range(stacks + 1):
        phi = math.pi * i / stacks
        for j in range(slices + 1):
            theta = math.tau * j / slices
            normal = (math.sin(phi) * math.cos(theta), math.cos(phi), math.sin(phi) * math.sin(theta))
            vertices.append((*[radius * x for x in normal], *normal, j / slices, i / stacks))
    for i in range(stacks):
        for j in range(slices):
            a = i * (slices + 1) + j
            indices += [a, a + 1, a + slices + 1, a + slices + 1, a + 1, a + slices + 2]
    return vertices, indices


def cylinder():
    vertices, indices = [], []
    slices, stacks = 20, 12
    for i in range(stacks + 1):
        radius, y = 0.5 - 0.2 * i / stacks, -1.5 + 3 * i / stacks
        for j in range(slices + 1):
            theta = math.tau * j / slices
            c, s = math.cos(theta), math.sin(theta)
            vertices.append((radius * c, y, radius * s, *normalize((c, 0.2 / 3, s)),
                             j / slices, 1 - i / stacks))
    for i in range(stacks):
        for j in range(slices):
            a = i * (slices + 1) + j
            indices += [a, a + slices + 1, a + 1, a + 1, a + slices + 1, a + slices + 2]
    for y, radius, sign in [(1.5, 0.3, 1), (-1.5, 0.5, -1)]:
        center = len(vertices)
        vertices.append((0, y, 0, 0, sign, 0, 0.5, 0.5))
        for j in range(slices + 1):
            theta = math.tau * j / slices
            x, z = radius * math.cos(theta), radius * math.sin(theta)
            vertices.append((x, y, z, 0, sign, 0, x / 3 + 0.5, z / 3 + 0.5))
        for j in range(slices):
            indices += [center, center + j + 1, center + j + 2]
    return vertices, indices


def quad(width, height):
    return ([(-width / 2, -height / 2, 0, 0, 0, -1, 0, 1),
             (-width / 2, height / 2, 0, 0, 0, -1, 0, 0),
             (width / 2, height / 2, 0, 0, 0, -1, 1, 0),
             (width / 2, -height / 2, 0, 0, 0, -1, 1, 1)], [0, 1, 2, 0, 2, 3])


def skull(path):
    text = path.read_text()
    counts = re.search(r"VertexCount:\s*(\d+)\s*TriangleCount:\s*(\d+)", text)
    vertices_text, indices_text = re.findall(r"\{([^{}]+)\}", text)
    values = [float(x) for x in vertices_text.split()]
    vertices = [tuple(values[i:i + 6]) + (0, 0) for i in range(0, len(values), 6)]
    return vertices[:int(counts[1])], [int(x) for x in indices_text.split()]


def multiply(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def transform(v, matrix, w):
    return tuple(sum(x * matrix[i][j] for i, x in enumerate((*v, w))) for j in range(3))


def slerp(a, b, t):
    dot = sum(x * y for x, y in zip(a, b))
    if dot < 0:
        b, dot = [-x for x in b], -dot
    if dot > 0.9995:
        return normalize([x + t * (y - x) for x, y in zip(a, b)])
    angle = math.acos(min(1, dot))
    return [(x * math.sin((1 - t) * angle) + y * math.sin(t * angle)) / math.sin(angle)
            for x, y in zip(a, b)]


def affine(position, scale, quaternion):
    x, y, z, w = normalize(quaternion)
    rows = [[1 - 2 * (y*y + z*z), 2 * (x*y + z*w), 2 * (x*z - y*w), 0],
            [2 * (x*y - z*w), 1 - 2 * (x*x + z*z), 2 * (y*z + x*w), 0],
            [2 * (x*z + y*w), 2 * (y*z - x*w), 1 - 2 * (x*x + y*y), 0]]
    return [[value * scale[i] for value in row] for i, row in enumerate(rows)] + [[*position, 1]]


def soldiers(path):
    """Bake the two poses that DefaultScene uses at time 0 (Take1 at 0 and .5s)."""
    text = path.read_text()
    source = re.findall(r"Position: ([^\n]+)\nTangent: [^\n]+\nNormal: ([^\n]+)\n"
                        r"Tex-Coords: ([^\n]+)\nBlendWeights: ([^\n]+)\nBlendIndices: ([^\n]+)", text)
    source = [(list(map(float, p.split())), list(map(float, n.split())), list(map(float, uv.split())),
               list(map(float, weights.split())), list(map(int, bones.split())))
              for p, n, uv, weights, bones in source]
    indices_text = re.search(r"\*+Triangles\*+\s*(.*?)\*+BoneOffsets", text, re.S)[1]
    indices = [int(x) for x in indices_text.split()]
    subsets = [tuple(map(int, row)) for row in re.findall(
        r"SubsetID: (\d+) VertexStart: (\d+) VertexCount: (\d+) FaceStart: (\d+) FaceCount: (\d+)", text)]
    offsets = [[list(map(float, row.split()))[i:i + 4] for i in range(0, 16, 4)]
               for row in re.findall(r"^BoneOffset\d+ ([^\n]+)", text, re.M)]
    parents = [int(x) for x in re.findall(r"ParentIndexOfBone\d+: (-?\d+)", text)]
    animations = []
    for block in re.findall(r"Bone\d+ #Keyframes: \d+\s*\{([^}]+)\}", text):
        animations.append([(float(time), list(map(float, pos.split())), list(map(float, scale.split())),
                            list(map(float, quat.split()))) for time, pos, scale, quat in re.findall(
                                r"Time: (\S+) Pos: (.*?) Scale: (.*?) Quat: ([^\n]+)", block)])
    materials = re.findall(r"Name: (\S+)\nDiffuse: ([^\n]+).*?DiffuseMap: (\S+)", text, re.S)
    result = []
    for pose, time in enumerate([0.0, 0.5]):
        worlds = []
        for parent, keys in zip(parents, animations):
            index = max(0, min(len(keys) - 2, bisect.bisect_right([k[0] for k in keys], time) - 1))
            a, b = keys[index:index + 2]
            t = max(0, min(1, (time - a[0]) / (b[0] - a[0])))
            position = [x + t * (y - x) for x, y in zip(a[1], b[1])]
            scale = [x + t * (y - x) for x, y in zip(a[2], b[2])]
            local = affine(position, scale, slerp(a[3], b[3], t))
            worlds.append(local if parent < 0 else multiply(local, worlds[parent]))
        matrices = [multiply(offset, world) for offset, world in zip(offsets, worlds)]
        vertices = []
        for p, n, uv, weights, bones in source:
            positions = [transform(p, matrices[b], 1) for b in bones]
            normals = [transform(n, matrices[b], 0) for b in bones]
            position = [sum(weights[k] * positions[k][j] for k in range(4)) for j in range(3)]
            normal = normalize([sum(weights[k] * normals[k][j] for k in range(4)) for j in range(3)])
            vertices.append((*position, *normal, *uv))
        for subset, material in zip(subsets, materials):
            number, start, count, first_face, faces = subset
            selected = indices[first_face * 3:(first_face + faces) * 3]
            remap, compact, local_indices = {}, [], []
            for original in selected:
                if original not in remap:
                    remap[original] = len(compact)
                    compact.append(vertices[original])
                local_indices.append(remap[original])
            result.append((f"Soldier{pose}_{number}", (compact, local_indices), material))
    return result


class Exporter:
    def __init__(self, source, assets):
        self.source, self.assets = source, assets
        self.folder = assets / "RendererLab"
        for name in ["Meshes", "Materials", "Textures"]:
            (self.folder / name).mkdir(parents=True, exist_ok=True)
        self.namespace = uuid.UUID(re.search(r"namespace (\S+)", (assets / "MEngine.project").read_text())[1])
        self.records, self.meshes, self.materials, self.objects = [], [], [], []

    def asset(self, relative, kind, dependencies=()):
        name = "RendererLab/" + relative
        key = str(uuid.uuid5(self.namespace, name + "\n"))
        record = [f'asset "MEngine.Asset.{kind}" "{name}" "" "" 1 0 {len(dependencies)}']
        record += [f'dependency "RendererLab/{dependency}" ""' for dependency in dependencies]
        self.records.append("\n".join(record + ["end_asset"]))
        return key

    def texture(self, name, layer=0):
        filename = f"{name}{layer if name == 'treeArray2' else ''}.png"
        width, height, pixels = read_dds(self.source / "Assets/Textures" / (name + ".dds"), layer)
        png(self.folder / "Textures" / filename, width, height, pixels)
        return self.asset("Textures/" + filename, "Texture2D"), "Textures/" + filename

    def surface_textures(self, normal_name, roughness_floor):
        """Preserve the matching authored normal map and author relative roughness around its grooves."""
        width, height, pixels = read_dds(self.source / "Assets/Textures" / (normal_name + ".dds"))
        png(self.folder / f"Textures/{normal_name}.png", width, height, pixels)
        normal = (self.asset(f"Textures/{normal_name}.png", "Texture2D"), f"Textures/{normal_name}.png")
        packed = bytearray(width * height * 4)
        for i in range(width * height):
            nz = pixels[i * 4 + 2] / 255 * 2 - 1
            groove = min(1, max(0, (1 - nz) * 5))
            roughness = roughness_floor + (1 - roughness_floor) * groove
            packed[i * 4:i * 4 + 4] = bytes((255, round(roughness * 255), 255, 255))
        name = normal_name.removesuffix("_nmap") + "_mr"
        png(self.folder / f"Textures/{name}.png", width, height, packed)
        mr = (self.asset(f"Textures/{name}.png", "Texture2D"), f"Textures/{name}.png")
        return normal, mr

    def mesh(self, name, geometry):
        vertices, indices = geometry
        vertex_bytes = b"".join(struct.pack("<8f", *vertex) for vertex in vertices)
        index_bytes = struct.pack(f"<{len(indices)}H", *indices)
        (self.folder / f"Meshes/{name}.bin").write_bytes(vertex_bytes + index_bytes)
        minimum = [min(v[i] for v in vertices) for i in range(3)]
        maximum = [max(v[i] for v in vertices) for i in range(3)]
        gltf = {"asset": {"version": "2.0", "generator": "MEngine RendererLab scene export"},
                "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0}],
                "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                                             "indices": 3, "material": 0}]}], "materials": [{}],
                "buffers": [{"uri": name + ".bin", "byteLength": len(vertex_bytes) + len(index_bytes)}],
                "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": len(vertex_bytes), "byteStride": 32},
                                {"buffer": 0, "byteOffset": len(vertex_bytes), "byteLength": len(index_bytes)}],
                "accessors": [{"bufferView": 0, "byteOffset": 0, "componentType": 5126,
                               "count": len(vertices), "type": "VEC3", "min": minimum, "max": maximum},
                              {"bufferView": 0, "byteOffset": 12, "componentType": 5126,
                               "count": len(vertices), "type": "VEC3"},
                              {"bufferView": 0, "byteOffset": 24, "componentType": 5126,
                               "count": len(vertices), "type": "VEC2"},
                              {"bufferView": 1, "componentType": 5123, "count": len(indices), "type": "SCALAR"}]}
        (self.folder / f"Meshes/{name}.gltf").write_text(json.dumps(gltf, indent=2) + "\n", encoding="utf-8")
        identifier = len(self.meshes) + 1
        self.meshes.append((identifier, self.asset(f"Meshes/{name}.gltf", "StaticMesh")))
        return identifier

    def material(self, name, texture=None, color=(1, 1, 1, 1), uv=(1, 1), cutoff=0,
                 metallic=0, roughness=1, normal=None, mr=None, normal_scale=1):
        document = {"base_color": color, "metallic": metallic, "roughness": roughness,
                    "uv_scale": uv, "alpha_cutoff": cutoff,
                    "filter": "linear", "address": "repeat" if uv != (1, 1) else "clamp"}
        if texture:
            document["base_color_texture"] = texture[0]
        if normal:
            document["normal_texture"] = normal[0]
            document["normal_scale"] = normal_scale
        if mr:
            document["metallic_roughness_texture"] = mr[0]
        (self.folder / f"Materials/{name}.material").write_text(json.dumps(document, indent=2) + "\n")
        identifier = len(self.materials) + 1
        self.materials.append((identifier, self.asset(f"Materials/{name}.material", "Material",
                                                    [item[1] for item in (texture, normal, mr) if item])))
        return identifier

    def object(self, number, name, mesh=None, material=None, position=(0, 0, 0),
               rotation=(0, 0, 0), scale=(1, 1, 1), parent=0, components=()):
        def values(items):
            return " ".join(format(value, ".9g") for value in items)
        payloads = [("Transform", values((*position, *rotation, *scale)))]
        if mesh is not None:
            payloads.append(("MeshRenderer", f"{mesh} {material} 1"))
        payloads += list(components)
        lines = [f'object {number} "{name}"', f"parent {parent}", f"component_count {len(payloads)}"]
        for kind, payload in payloads:
            lines += [f'component "MEngine.Runtime.{kind}Component" 1 {len(payload.encode())}', payload]
        self.objects.append("\n".join(lines + ["end_object"]))

    def finish(self):
        scene = ["MENGINE_SCENE 5", "scene_state 0",
                 "render_settings -0.57735 0.57735 -0.57735 4 0.03 1.2 0",
                 "lighting_settings 1 0.97 0.92 0.8 0 1 80", "local_lights 3",
                 "light 0 -4 3 -4 9 0 -1 0 30 1 0.35 0.12 20 35",
                 "light 0 5 4 3 10 0 -1 0 35 0.2 0.45 1 20 35",
                 "light 1 0 6 5 12 0 -1 -0.5 60 1 0.85 0.6 18 30",
                 f"mesh_assets {len(self.meshes)}"]
        scene += [f"{number} {key}" for number, key in self.meshes]
        scene += [f"material_assets {len(self.materials)}"]
        scene += [f"{number} {key}" for number, key in self.materials]
        scene += [f"object_count {len(self.objects)}", *self.objects, "end_scene"]
        (self.assets / "Scenes/RendererLab.mscene").write_text("\n".join(scene) + "\n", encoding="utf-8", newline="\n")
        registry = self.assets / "AssetRegistry.mregistry"
        original = re.findall(r'^asset "[^\n]+\n.*?^end_asset$', registry.read_text(), re.M | re.S)
        original = [record for record in original if '"RendererLab/' not in record.splitlines()[0]]
        records = original + self.records
        registry.write_text(f"MENGINE_ASSET_REGISTRY 1\nasset_count {len(records)}\n" +
                            "\n".join(records) + "\nend_registry\n", encoding="utf-8", newline="\n")
        project = self.assets / "MEngine.project"
        project.write_text(re.sub(r'startup_scene "[^"]*"', 'startup_scene "Scenes/RendererLab.mscene"',
                                  project.read_text()), encoding="utf-8", newline="\n")
        (self.folder / "SOURCE.md").write_text(
            "# RendererLab 场景资产来源\n\n"
            "模型、纹理来自本地 RendererLab/Assets，原始来源为 Frank Luna 的 d3d12book 配套资产。\n"
            "原作者 Frank Luna；沿用原资产使用条件，未声明额外分发授权。\n"
            "场景尺寸、位置和材质颜色参考 RendererLab/App/DefaultScene.cpp。\n\n"
            "导出脚本 Tools/ExportRendererLabScene.py 将 skull.txt 转为 glTF、DDS 转为 PNG，"
            "并把 soldier.m3d 的 Take1 在 0 秒和 0.5 秒的姿态烘焙为普通静态网格。\n"
            "立柱、球体、网格地面和曲面导出为普通三角形网格；树木使用固定朝向的透明裁剪面片。\n"
            "当前保持原场景布局和几何内容，材质已按砖石、地砖、金属、织物和皮肤配置金属度与粗糙度。"
            "这些参数为外观初值，不是实测值，详见 docs/Materials.md。\n"
            "砖块与地砖法线来自 https://github.com/d3dcoder/d3d12book/tree/master/Textures 中对应的 "
            "bricks2_nmap.dds 和 tile_nmap.dds；打包粗糙度是导出脚本根据沟槽分布制作的外观近似，非实测。\n"
            "水面为静态几何，水和冰使用低粗糙度不透明非金属近似；"
            "不包含平面反射、SSAO、水波、透射或实时骨骼动画。\n"
            "天空使用 MEngine 当前天空盒，并参与 IBL；方向光投射级联阴影，场景中另有点光和聚光。\n", encoding="utf-8")
        print(f"Exported RendererLab: {len(self.objects)} objects, {len(self.meshes)} meshes, "
              f"{len(self.materials)} materials; startup scene is Scenes/RendererLab.mscene")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--asset-root", type=Path, default=Path(__file__).resolve().parents[1] / "Assets")
    options = parser.parse_args()
    exporter = Exporter(options.source, options.asset_root)
    textures = {name: exporter.texture(name) for name in
                ["bricks2", "tile", "water1", "WireFence", "ice", "head_diff", "jacket_diff", "pants_diff", "upBody_diff"]}
    trees = [exporter.texture("treeArray2", layer) for layer in range(3)]
    brick_normal, brick_mr = exporter.surface_textures("bricks2_nmap", 0.82)
    tile_normal, tile_mr = exporter.surface_textures("tile_nmap", 0.7)
    brick = exporter.material("Bricks", textures["bricks2"], roughness=0.9,
                              normal=brick_normal, mr=brick_mr, normal_scale=0.7)
    tile = exporter.material("GroundTiles", textures["tile"], uv=(12, 12), roughness=0.45,
                             normal=tile_normal, mr=tile_mr, normal_scale=0.5)
    metal = exporter.material("Polished", color=(0.8, 0.8, 0.85, 1), metallic=1, roughness=0.22)
    water = exporter.material("Water", textures["water1"], color=(0.5, 0.75, 0.95, 1), roughness=0.08)
    fence = exporter.material("Fence", textures["WireFence"], cutoff=0.1, metallic=1, roughness=0.5)
    tree_materials = [exporter.material(f"Tree{i}", texture, cutoff=0.1, roughness=0.85)
                      for i, texture in enumerate(trees)]
    ice = exporter.material("IcePanel", textures["ice"], color=(0.72, 0.85, 1, 1), roughness=0.12)
    patch_tile = exporter.material("PatchTiles", textures["tile"], roughness=0.45,
                                   normal=tile_normal, mr=tile_mr, normal_scale=0.5)
    box_mesh = exporter.mesh("Box", box())
    ball_mesh = exporter.mesh("Sphere", sphere())
    column_mesh = exporter.mesh("Column", cylinder())
    ground_mesh = exporter.mesh("Ground", grid(40, 40, 40, 40))
    skull_mesh = exporter.mesh("Skull", skull(options.source / "Assets/Models/skull.txt"))
    water_mesh = exporter.mesh("Water", grid(7.75, 7.75, 32, 32))
    tree_mesh = exporter.mesh("TreeCard", quad(3, 5))
    patch_mesh = exporter.mesh("QuadPatch", grid(4, 4, 24, 24))
    # The source Bezier patch has a raised central 2x2 group of control points.
    def bezier_height(x, z):
        u, v = (x + 2.1) / 4.2, (2.1 - z) / 4.2
        return 27 * u * (1 - u) * v * (1 - v)
    bezier_mesh = exporter.mesh("BezierPatch", grid(4.2, 4.2, 32, 32, bezier_height))
    mirror_mesh = exporter.mesh("IcePanel", quad(6, 6))
    exporter.object(1000, "Main Camera", position=(0, 7, -22), rotation=(math.degrees(math.atan(5 / 25)), 0, 0),
                    components=[("Camera", "45 0.5 500 1")])
    exporter.object(1, "Tiled Ground", ground_mesh, tile)
    exporter.object(2, "Brick Pedestal", box_mesh, brick, position=(0, 0.75, 0), scale=(2, 1, 2))
    exporter.object(3, "Skull", skull_mesh, metal, position=(0, 2.5, 0), scale=(0.5, 0.5, 0.5))
    for row in range(5):
        for side, x in enumerate([-5, 5]):
            z = -8 + row * 4
            exporter.object(100 + row * 2 + side, f"Brick Column {row + 1} {'Left' if side == 0 else 'Right'}",
                            column_mesh, brick, position=(x, 1.5, z))
            exporter.object(200 + row * 2 + side, f"Column Sphere {row + 1} {'Left' if side == 0 else 'Right'}",
                            ball_mesh, metal, position=(x, 3.6, z))
    exporter.object(4, "Wire Fence", box_mesh, fence, position=(-2.5, 1.5, 3), scale=(1.2, 2, 0.15))
    exporter.object(5, "Water Surface Left", water_mesh, water, position=(-10, 0.4, -3))
    exporter.object(6, "Water Surface Right", water_mesh, water, position=(10, 0.4, -3))
    for i in range(8):
        exporter.object(300 + i, f"Tree {i + 1}", tree_mesh, tree_materials[i % 3], position=(i * 3 - 10, 2.5, 16))
    exporter.object(8, "Quad Surface", patch_mesh, brick, position=(-9, 2, 8))
    exporter.object(9, "Bezier Surface", bezier_mesh, patch_tile, position=(9, 1, 8))
    exporter.object(10, "Ice Panel", mirror_mesh, ice, position=(0, 3, 12))
    for pose in range(2):
        exporter.object(20 + pose, f"Soldier {'Left' if pose == 0 else 'Right'}", position=(-2 if pose == 0 else 2, 0, -5),
                        rotation=(0, 180 if pose == 0 else 0, 0), scale=(0.05, 0.05, -0.05))
    material_ids = {}
    soldier_roughness = {"soldier_head": 0.6, "soldier_jacket": 0.8, "soldier_pants": 0.9,
                        "soldier_upBody1": 0.7, "soldier_upBody2": 0.7}
    for name, geometry, (material_name, color_text, texture_name) in soldiers(options.source / "Assets/Models/soldier.m3d"):
        mesh = exporter.mesh(name, geometry)
        if material_name not in material_ids:
            material_ids[material_name] = exporter.material(material_name, textures[Path(texture_name).stem],
                                                           color=(*map(float, color_text.split()), 1),
                                                           roughness=soldier_roughness[material_name])
        pose, part = map(int, re.findall(r"\d+", name))
        exporter.object(400 + pose * 10 + part, material_name, mesh, material_ids[material_name], parent=20 + pose)
    exporter.finish()


if __name__ == "__main__":
    main()
