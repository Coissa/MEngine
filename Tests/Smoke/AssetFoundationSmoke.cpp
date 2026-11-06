#include <Assets/Registry/AssetId.h>
#include <Assets/Registry/AssetKey.h>
#include <Assets/Registry/AssetRegistry.h>
#include <Assets/Registry/AssetRegistrySerializer.h>
#include <Assets/Registry/ProjectDescriptor.h>
#include <Runtime/EngineServices.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace
{
    [[nodiscard]] GE::Asset::AssetId Parse(const char* text)
    {
        auto result = GE::Asset::ParseAssetId(text);
        if (!result)
            return {};
        return std::move(result).Value();
    }

    [[nodiscard]] GE::Asset::AssetKey Key(const char* path, const char* subasset = "")
    {
        auto result = GE::Asset::MakeAssetKey(path, subasset);
        if (!result)
            return {};
        return std::move(result).Value();
    }

    [[nodiscard]] bool TestUuidAndPaths()
    {
        const GE::Asset::AssetId dns_namespace = Parse("6ba7b810-9dad-11d1-80b4-00c04fd430c8");
        if (!dns_namespace.IsValid() ||
            GE::Asset::ToString(GE::Asset::MakeAssetIdV5(dns_namespace, "www.widgets.com")) !=
                "21f7f8de-8051-5b89-8680-0195ef798b6a" ||
            GE::Asset::ParseAssetId("00000000-0000-0000-0000-000000000000"))
        {
            return false;
        }

        const GE::Asset::AssetId other_namespace = Parse("a1b64b08-8c90-40f9-8256-9c21aaa91154");
        if (GE::Asset::MakeAssetIdV5(dns_namespace, "same") == GE::Asset::MakeAssetIdV5(other_namespace, "same"))
        {
            return false;
        }

        auto normalized = GE::Asset::MakeAssetKey("Textures\\Sky/./Generated", "cubemap");
        return normalized && normalized.Value().SourcePath() == "Textures/Sky/Generated" &&
               normalized.Value().SubassetKey() == "cubemap" && !GE::Asset::MakeAssetKey("../Outside") &&
               !GE::Asset::MakeAssetKey("D:/Absolute") && !GE::Asset::MakeAssetKey("");
    }

    [[nodiscard]] bool TestRegistry()
    {
        const GE::Asset::AssetId project_namespace = Parse("a1b64b08-8c90-40f9-8256-9c21aaa91154");
        const GE::Asset::AssetKey a = Key("Assets/A");
        const GE::Asset::AssetKey b = Key("Assets/B", "mesh");
        GE::Asset::AssetRegistry registry(project_namespace);
        auto a_id = registry.Add({.key = a, .stable_type_name = "MEngine.Asset.Texture", .dependencies = {b}});
        auto b_id = registry.Add({.key = b, .stable_type_name = "MEngine.Asset.Mesh", .dependencies = {a}});
        if (!a_id || !b_id || registry.Add({.key = a, .stable_type_name = "MEngine.Asset.Texture"}))
        {
            return false;
        }
#ifdef _WIN32
        if (registry.Add({.key = Key("assets/b", "mesh"), .stable_type_name = "MEngine.Asset.Mesh"}))
        {
            return false;
        }
#endif
        if (!registry.Freeze() || registry.GetAll().size() != 2 || registry.Find(a_id.Value()) == nullptr ||
            registry.Find(b) == nullptr || registry.GetDependencies(a_id.Value()).size() != 1 ||
            registry.GetReferencers(a_id.Value()).size() != 1 ||
            registry.Add({.key = Key("Assets/C"), .stable_type_name = "MEngine.Asset.Texture"}))
        {
            return false;
        }

        GE::Asset::AssetRegistry dangling(project_namespace);
        if (!dangling.Add({.key = a, .stable_type_name = "MEngine.Asset.Texture", .dependencies = {Key("Missing")}}) ||
            dangling.Freeze())
        {
            return false;
        }
        return true;
    }

    [[nodiscard]] bool TestSerialization()
    {
        const GE::Asset::AssetId project_namespace = Parse("a1b64b08-8c90-40f9-8256-9c21aaa91154");
        GE::Asset::ProjectDescriptor descriptor{project_namespace};
        std::stringstream project_text;
        if (!GE::Asset::ProjectDescriptorSerializer::Save(descriptor, project_text))
        {
            return false;
        }
        auto loaded_project = GE::Asset::ProjectDescriptorSerializer::Load(project_text);
        if (!loaded_project || loaded_project.Value().namespace_id != project_namespace)
        {
            return false;
        }

        GE::Asset::AssetRegistry registry(project_namespace);
        const GE::Asset::AssetKey texture = Key("Textures/A.png");
        const GE::Asset::AssetKey material = Key("Models/A.gltf", "Material0");
        if (!registry.Add({.key = texture,
                           .stable_type_name = "MEngine.Asset.Texture",
                           .cooked_relative_path = "Textures/A.texture",
                           .importer_version = 2,
                           .cooked_version = 3}) ||
            !registry.Add({.key = material, .stable_type_name = "MEngine.Asset.Material", .dependencies = {texture}}) ||
            !registry.Freeze())
        {
            return false;
        }

        std::stringstream first;
        if (!GE::Asset::AssetRegistrySerializer::Save(registry, first))
            return false;
        const std::string saved = first.str();
        std::stringstream input(saved);
        auto loaded = GE::Asset::AssetRegistrySerializer::Load(input, project_namespace);
        if (!loaded || loaded.Value().GetAll().size() != 2 || loaded.Value().Find(material) == nullptr)
        {
            return false;
        }
        std::stringstream second;
        if (!GE::Asset::AssetRegistrySerializer::Save(loaded.Value(), second) || second.str() != saved)
        {
            return false;
        }

        const std::string invalid_cases[] = {"MENGINE_ASSET_REGISTRY 2\nasset_count 0\nend_registry\n",
                                             "MENGINE_ASSET_REGISTRY 1\nasset_count 0\nunknown\n",
                                             "MENGINE_ASSET_REGISTRY 1\nasset_count 1\n",
                                             "MENGINE_ASSET_REGISTRY 1\nasset_count 2\n"
                                             "asset \"Type\" \"A\" \"\" \"\" 0 0 0\nend_asset\n"
                                             "asset \"Type\" \"A\" \"\" \"\" 0 0 0\nend_asset\n"
                                             "end_registry\n",
                                             "MENGINE_ASSET_REGISTRY 1\nasset_count 1\n"
                                             "asset \"Type\" \"A\" \"\" \"\" 0 0 1\n"
                                             "dependency \"Missing\" \"\"\nend_asset\nend_registry\n"};
        for (const std::string& invalid : invalid_cases)
        {
            std::stringstream invalid_input(invalid);
            if (GE::Asset::AssetRegistrySerializer::Load(invalid_input, project_namespace))
            {
                return false;
            }
        }

        const std::filesystem::path directory = std::filesystem::temp_directory_path() / "MEngineAssetAtomicSave";
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
        std::filesystem::create_directories(directory);
        const std::filesystem::path target = directory / "Registry.mregistry";
        {
            std::ofstream existing(target);
            existing << "sentinel";
        }
        GE::Asset::AssetRegistry unfrozen(project_namespace);
        if (GE::Asset::AssetRegistrySerializer::SaveToFile(unfrozen, target))
            return false;
        std::ifstream preserved(target);
        std::string preserved_text;
        preserved >> preserved_text;
        std::filesystem::remove_all(directory, ignored);
        return preserved_text == "sentinel";
    }

    [[nodiscard]] bool TestRegistryFiles()
    {
        const std::filesystem::path root = MENGINE_TEST_ASSET_ROOT;
        auto project = GE::Asset::ProjectDescriptorSerializer::LoadFromFile(root / "MEngine.project");
        if (!project)
            return false;
        auto assets = GE::Asset::AssetRegistrySerializer::LoadFromFile(root / "AssetRegistry.mregistry",
                                                                       project.Value().namespace_id);
        auto key = GE::Asset::MakeAssetKey("Textures/Sky/GeneratedDayBlack_v1");
        return assets && key && assets.Value().Find(key.Value());
    }

} // namespace

int main()
{
    if (!TestUuidAndPaths())
        return 1;
    if (!TestRegistry())
        return 2;
    if (!TestSerialization())
        return 3;
    if (!TestRegistryFiles())
        return 4;
    std::cout << "[AssetFoundationSmoke] Passed\n";
    return 0;
}
