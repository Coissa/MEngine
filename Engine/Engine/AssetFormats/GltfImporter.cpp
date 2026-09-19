#include "GltfImporter.h"

#include <Engine/Core/Math/quaternion.h>
#include <ThirdParty/nlohmann/json.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <string_view>
#include <unordered_set>

namespace GE::AssetFormats
{
    namespace
    {
        using Json = nlohmann::json;

        struct SourceDocument final
        {
            Json json;
            std::vector<FileSystem::ByteBuffer> buffers;
        };

        [[nodiscard]] Core::EngineError GltfError(
            Core::ErrorCode code, std::string message,
            const FileSystem::LogicalPath& path)
        {
            return {.domain = Core::ErrorDomain::Asset, .code = code,
                .operation = "ImportGltf", .message = std::move(message),
                .resource = path.String()};
        }

        [[nodiscard]] std::uint32_t U32(
            std::span<const std::byte> bytes, std::size_t offset)
        {
            std::uint32_t value = 0;
            for (std::size_t index = 0; index < 4; ++index)
                value |= std::to_integer<std::uint32_t>(bytes[offset + index])
                    << (index * 8);
            return value;
        }

        [[nodiscard]] Core::Result<FileSystem::LogicalPath> ResolveUri(
            const FileSystem::LogicalPath& source, std::string_view uri)
        {
            if (uri.empty() || uri.find(':') != std::string_view::npos
                || uri.find('%') != std::string_view::npos)
                return Core::Result<FileSystem::LogicalPath>::Failure(
                    GltfError(Core::ErrorCode::InvalidArgument,
                        "Only plain relative external URIs are supported",
                        source));
            const std::size_t slash = source.String().find_last_of('/');
            std::string combined = slash == std::string::npos
                ? std::string(uri)
                : source.String().substr(0, slash + 1) + std::string(uri);
            return FileSystem::NormalizeLogicalPath(combined);
        }

        [[nodiscard]] Core::Result<SourceDocument> LoadDocument(
            const FileSystem::IReadOnlyFileMount& mount,
            const FileSystem::LogicalPath& path)
        {
            auto source = mount.ReadAll(path);
            if (!source)
                return Core::Result<SourceDocument>::Failure(source.Error());
            SourceDocument result;
            std::optional<FileSystem::ByteBuffer> glb_binary;
            try
            {
                const bool glb = source.Value().size() >= 12
                    && U32(source.Value(), 0) == 0x46546c67u;
                if (glb)
                {
                    if (U32(source.Value(), 4) != 2
                        || U32(source.Value(), 8) != source.Value().size())
                        return Core::Result<SourceDocument>::Failure(GltfError(
                            Core::ErrorCode::ParseFailure,
                            "GLB header is invalid", path));
                    std::size_t offset = 12;
                    bool found_json = false;
                    while (offset < source.Value().size())
                    {
                        if (source.Value().size() - offset < 8)
                            return Core::Result<SourceDocument>::Failure(
                                GltfError(Core::ErrorCode::ParseFailure,
                                    "GLB chunk header is truncated", path));
                        const std::uint32_t length = U32(source.Value(), offset);
                        const std::uint32_t type = U32(source.Value(), offset + 4);
                        offset += 8;
                        if (length > source.Value().size() - offset)
                            return Core::Result<SourceDocument>::Failure(
                                GltfError(Core::ErrorCode::ParseFailure,
                                    "GLB chunk is truncated", path));
                        const auto chunk = std::span(source.Value()).subspan(
                            offset, length);
                        if (type == 0x4e4f534au && !found_json)
                        {
                            std::string text(reinterpret_cast<const char*>(
                                chunk.data()), chunk.size());
                            while (!text.empty() && text.back() == '\0')
                                text.pop_back();
                            result.json = Json::parse(text);
                            found_json = true;
                        }
                        else if (type == 0x004e4942u && !glb_binary)
                            glb_binary = FileSystem::ByteBuffer(
                                chunk.begin(), chunk.end());
                        offset += length;
                    }
                    if (!found_json)
                        return Core::Result<SourceDocument>::Failure(GltfError(
                            Core::ErrorCode::ParseFailure,
                            "GLB has no JSON chunk", path));
                }
                else
                {
                    const std::string text(reinterpret_cast<const char*>(
                        source.Value().data()), source.Value().size());
                    result.json = Json::parse(text);
                }

                if (result.json.at("asset").at("version").get<std::string>()
                    != "2.0")
                    return Core::Result<SourceDocument>::Failure(GltfError(
                        Core::ErrorCode::UnsupportedVersion,
                        "Only glTF 2.0 is supported", path));
                const Json& buffers = result.json.at("buffers");
                for (std::size_t index = 0; index < buffers.size(); ++index)
                {
                    FileSystem::ByteBuffer bytes;
                    if (buffers[index].contains("uri"))
                    {
                        auto resolved = ResolveUri(path,
                            buffers[index].at("uri").get<std::string>());
                        if (!resolved)
                            return Core::Result<SourceDocument>::Failure(
                                resolved.Error());
                        auto loaded = mount.ReadAll(resolved.Value());
                        if (!loaded)
                            return Core::Result<SourceDocument>::Failure(
                                loaded.Error());
                        bytes = std::move(loaded).Value();
                    }
                    else if (index == 0 && glb_binary)
                        bytes = std::move(*glb_binary);
                    else
                        return Core::Result<SourceDocument>::Failure(GltfError(
                            Core::ErrorCode::ParseFailure,
                            "Buffer has no supported data source", path));
                    if (bytes.size()
                        < buffers[index].at("byteLength").get<std::size_t>())
                        return Core::Result<SourceDocument>::Failure(GltfError(
                            Core::ErrorCode::ParseFailure,
                            "Buffer is shorter than declared", path));
                    result.buffers.push_back(std::move(bytes));
                }
            }
            catch (const std::exception& error)
            {
                return Core::Result<SourceDocument>::Failure(GltfError(
                    Core::ErrorCode::ParseFailure,
                    std::string("Invalid glTF structure: ") + error.what(),
                    path));
            }
            return Core::Result<SourceDocument>::Success(std::move(result));
        }

        struct AccessorView final
        {
            const std::byte* data = nullptr;
            std::size_t stride = 0;
            std::size_t count = 0;
            std::uint32_t component_type = 0;
            std::string type;
        };

        [[nodiscard]] AccessorView Accessor(
            const SourceDocument& document, std::size_t index)
        {
            const Json& accessor = document.json.at("accessors").at(index);
            if (accessor.contains("sparse"))
                throw std::runtime_error("Sparse accessors are not supported");
            const Json& view = document.json.at("bufferViews").at(
                accessor.at("bufferView").get<std::size_t>());
            const std::size_t buffer_index = view.at("buffer").get<std::size_t>();
            const std::size_t view_offset = view.value("byteOffset", 0u);
            const std::size_t view_length = view.at("byteLength");
            const std::size_t accessor_offset = accessor.value("byteOffset", 0u);
            const std::uint32_t component = accessor.at("componentType");
            const std::string type = accessor.at("type");
            const std::size_t components = type == "SCALAR" ? 1
                : type == "VEC2" ? 2 : type == "VEC3" ? 3
                : type == "VEC4" ? 4 : 0;
            const std::size_t component_size = component == 5126 ? 4
                : component == 5125 ? 4 : component == 5123 ? 2
                : component == 5121 ? 1 : 0;
            if (components == 0 || component_size == 0
                || buffer_index >= document.buffers.size())
                throw std::runtime_error("Accessor format is unsupported");
            const std::size_t element_size = components * component_size;
            const std::size_t stride = view.value("byteStride", element_size);
            const std::size_t count = accessor.at("count");
            const std::size_t buffer_size = document.buffers[buffer_index].size();
            if (view_offset > buffer_size
                || view_length > buffer_size - view_offset
                || accessor_offset > view_length
                || stride < element_size
                || (count != 0 && (view_length - accessor_offset < element_size
                    || (count - 1) > (view_length - accessor_offset
                        - element_size) / stride)))
                throw std::runtime_error("Accessor range is invalid");
            const std::size_t start = view_offset + accessor_offset;
            return {document.buffers[buffer_index].data() + start,
                stride, count, component, type};
        }

        [[nodiscard]] float FloatAt(const AccessorView& view,
            std::size_t element, std::size_t component)
        {
            float value = 0;
            std::memcpy(&value, view.data + element * view.stride
                + component * sizeof(float), sizeof(float));
            if (!std::isfinite(value))
                throw std::runtime_error("Accessor contains non-finite float");
            return value;
        }

        [[nodiscard]] std::uint32_t IndexAt(
            const AccessorView& view, std::size_t element)
        {
            const std::byte* data = view.data + element * view.stride;
            if (view.component_type == 5121)
                return std::to_integer<std::uint8_t>(*data);
            if (view.component_type == 5123)
            {
                std::uint16_t value = 0; std::memcpy(&value, data, 2); return value;
            }
            std::uint32_t value = 0; std::memcpy(&value, data, 4); return value;
        }

        [[nodiscard]] Math::Mat4 NodeTransform(const Json& node)
        {
            if (node.contains("matrix"))
            {
                const auto values = node.at("matrix").get<std::vector<float>>();
                if (values.size() != 16
                    || std::any_of(values.begin(), values.end(),
                        [](float value) { return !std::isfinite(value); }))
                    throw std::runtime_error("Node matrix is invalid");
                Math::Mat4 result;
                std::copy(values.begin(), values.end(), result.values.begin());
                return result;
            }
            Math::Vec3 translation{0, 0, 0}, scale{1, 1, 1};
            Math::Quaternion rotation;
            if (node.contains("translation"))
            {
                auto value = node.at("translation").get<std::vector<float>>();
                if (value.size() != 3) throw std::runtime_error("Bad translation");
                translation = {value[0], value[1], value[2]};
            }
            if (node.contains("scale"))
            {
                auto value = node.at("scale").get<std::vector<float>>();
                if (value.size() != 3) throw std::runtime_error("Bad scale");
                scale = {value[0], value[1], value[2]};
            }
            if (node.contains("rotation"))
            {
                auto value = node.at("rotation").get<std::vector<float>>();
                if (value.size() != 4) throw std::runtime_error("Bad rotation");
                rotation = {value[0], value[1], value[2], value[3]};
            }
            if (!Math::IsFinite(translation) || !Math::IsFinite(scale))
                throw std::runtime_error("Node TRS is not finite");
            return Math::ScaleMatrix(scale) * Math::RotationMatrix(rotation)
                * Math::TranslationMatrix(translation);
        }
    }

    Core::Result<ImportedGltf> ImportGltf(
        const FileSystem::IReadOnlyFileMount& mount,
        const FileSystem::LogicalPath& path)
    {
        auto loaded = LoadDocument(mount, path);
        if (!loaded) return Core::Result<ImportedGltf>::Failure(loaded.Error());
        const SourceDocument& document = loaded.Value();
        ImportedGltf result;
        try
        {
            const auto texture_uri = [&](std::size_t texture_index)
                -> std::optional<std::string>
            {
                const std::size_t image_index = document.json.at("textures")
                    .at(texture_index).at("source");
                const Json& image = document.json.at("images").at(image_index);
                if (!image.contains("uri"))
                    throw std::runtime_error("Embedded images are not supported");
                auto resolved = ResolveUri(path, image.at("uri").get<std::string>());
                if (!resolved) throw std::runtime_error(resolved.Error().message);
                return resolved.Value().String();
            };
            for (const Json& source : document.json.value("materials", Json::array()))
            {
                ImportedGltfMaterial material;
                const Json pbr = source.value("pbrMetallicRoughness", Json::object());
                const auto color = pbr.value("baseColorFactor",
                    std::vector<float>{1, 1, 1, 1});
                if (color.size() != 4) throw std::runtime_error("Bad base color");
                material.material.base_color = {color[0], color[1], color[2], color[3]};
                material.material.metallic = pbr.value("metallicFactor", 1.0F);
                material.material.roughness = pbr.value("roughnessFactor", 1.0F);
                const auto emissive = source.value("emissiveFactor",
                    std::vector<float>{0, 0, 0});
                if (emissive.size() != 3) throw std::runtime_error("Bad emissive");
                material.material.emissive = {emissive[0], emissive[1], emissive[2]};
                if (pbr.contains("baseColorTexture"))
                    material.base_color_texture_uri = texture_uri(
                        pbr.at("baseColorTexture").at("index"));
                if (pbr.contains("metallicRoughnessTexture"))
                    material.metallic_roughness_texture_uri = texture_uri(
                        pbr.at("metallicRoughnessTexture").at("index"));
                if (source.contains("normalTexture"))
                    material.normal_texture_uri = texture_uri(
                        source.at("normalTexture").at("index"));
                if (source.contains("emissiveTexture"))
                    material.emissive_texture_uri = texture_uri(
                        source.at("emissiveTexture").at("index"));
                result.materials.push_back(std::move(material));
            }
            if (result.materials.empty())
                result.materials.emplace_back();

            const Json& nodes = document.json.at("nodes");
            std::vector<std::int32_t> parents(nodes.size(), -1);
            for (std::size_t index = 0; index < nodes.size(); ++index)
                for (const std::size_t child : nodes[index].value("children",
                         std::vector<std::size_t>{}))
                {
                    if (child >= nodes.size() || parents[child] != -1)
                        throw std::runtime_error("Node parent graph is invalid");
                    parents[child] = static_cast<std::int32_t>(index);
                }
            std::vector<std::size_t> roots;
            const Json scenes = document.json.value("scenes", Json::array());
            if (!scenes.empty())
            {
                const std::size_t scene = document.json.value("scene", 0u);
                roots = scenes.at(scene).value("nodes", std::vector<std::size_t>{});
            }
            else
                for (std::size_t index = 0; index < nodes.size(); ++index)
                    if (parents[index] == -1) roots.push_back(index);

            std::unordered_set<std::size_t> visited;
            std::function<void(std::size_t, std::int32_t)> visit =
                [&](std::size_t source_index, std::int32_t parent)
            {
                if (source_index >= nodes.size() || !visited.insert(source_index).second)
                    throw std::runtime_error("Scene node graph is cyclic or repeated");
                const Json& node = nodes[source_index];
                StaticMeshNode output_node{.parent = parent,
                    .local_transform = NodeTransform(node),
                    .first_submesh = static_cast<std::uint32_t>(
                        result.mesh.submeshes.size())};
                const std::size_t output_index = result.mesh.nodes.size();
                result.mesh.nodes.push_back(output_node);
                if (node.contains("skin"))
                    throw std::runtime_error("Skin nodes are not supported");
                if (node.contains("mesh"))
                {
                    const Json& mesh = document.json.at("meshes").at(
                        node.at("mesh").get<std::size_t>());
                    for (const Json& primitive : mesh.at("primitives"))
                    {
                        if (primitive.value("mode", 4) != 4
                            || primitive.contains("targets")
                            || !primitive.contains("indices"))
                            throw std::runtime_error("Only indexed triangle primitives are supported");
                        const Json& attributes = primitive.at("attributes");
                        AccessorView positions = Accessor(document,
                            attributes.at("POSITION"));
                        AccessorView normals = Accessor(document,
                            attributes.at("NORMAL"));
                        if (positions.component_type != 5126 || positions.type != "VEC3"
                            || normals.component_type != 5126 || normals.type != "VEC3"
                            || positions.count != normals.count || positions.count == 0)
                            throw std::runtime_error("Position/normal accessors are invalid");
                        std::optional<AccessorView> texcoords;
                        if (attributes.contains("TEXCOORD_0"))
                        {
                            texcoords = Accessor(document, attributes.at("TEXCOORD_0"));
                            if (texcoords->component_type != 5126
                                || texcoords->type != "VEC2"
                                || texcoords->count != positions.count)
                                throw std::runtime_error("Texcoord accessor is invalid");
                        }
                        const std::uint32_t vertex_base = static_cast<std::uint32_t>(
                            result.mesh.vertices.size());
                        Math::Vec3 minimum{INFINITY, INFINITY, INFINITY};
                        Math::Vec3 maximum{-INFINITY, -INFINITY, -INFINITY};
                        for (std::size_t vertex = 0; vertex < positions.count; ++vertex)
                        {
                            StaticMeshVertex value{
                                .position = {FloatAt(positions, vertex, 0),
                                    FloatAt(positions, vertex, 1),
                                    FloatAt(positions, vertex, 2)},
                                .normal = {FloatAt(normals, vertex, 0),
                                    FloatAt(normals, vertex, 1),
                                    FloatAt(normals, vertex, 2)}};
                            if (texcoords) value.texcoord = {
                                FloatAt(*texcoords, vertex, 0),
                                FloatAt(*texcoords, vertex, 1)};
                            for (int axis = 0; axis < 3; ++axis)
                            {
                                minimum[axis] = std::min(minimum[axis], value.position[axis]);
                                maximum[axis] = std::max(maximum[axis], value.position[axis]);
                            }
                            result.mesh.vertices.push_back(value);
                        }
                        AccessorView indices = Accessor(document, primitive.at("indices"));
                        if (indices.type != "SCALAR" || indices.count == 0
                            || indices.count % 3 != 0
                            || (indices.component_type != 5121
                                && indices.component_type != 5123
                                && indices.component_type != 5125))
                            throw std::runtime_error("Index accessor is invalid");
                        const std::uint32_t first_index = static_cast<std::uint32_t>(
                            result.mesh.indices.size());
                        for (std::size_t index = 0; index < indices.count; ++index)
                        {
                            const std::uint32_t value = IndexAt(indices, index);
                            if (value >= positions.count)
                                throw std::runtime_error("Index is out of range");
                            result.mesh.indices.push_back(vertex_base + value);
                        }
                        const std::uint32_t material_slot =
                            primitive.value("material", 0u);
                        if (material_slot >= result.materials.size())
                            throw std::runtime_error("Material index is out of range");
                        result.mesh.submeshes.push_back({.first_index = first_index,
                            .index_count = static_cast<std::uint32_t>(indices.count),
                            .material_slot = material_slot,
                            .bounds_min = minimum, .bounds_max = maximum});
                    }
                }
                result.mesh.nodes[output_index].submesh_count =
                    static_cast<std::uint32_t>(result.mesh.submeshes.size())
                    - result.mesh.nodes[output_index].first_submesh;
                for (const std::size_t child : node.value("children",
                         std::vector<std::size_t>{}))
                    visit(child, static_cast<std::int32_t>(output_index));
            };
            for (const std::size_t root : roots) visit(root, -1);
            if (result.mesh.nodes.empty() || result.mesh.submeshes.empty())
                throw std::runtime_error("Scene contains no static mesh primitives");
        }
        catch (const std::exception& error)
        {
            return Core::Result<ImportedGltf>::Failure(GltfError(
                Core::ErrorCode::ParseFailure, error.what(), path));
        }
        return Core::Result<ImportedGltf>::Success(std::move(result));
    }
}
