#pragma once

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace GE::Tests
{
    class TestAssetDirectory final
    {
    public:
        explicit TestAssetDirectory(std::string_view name)
            : m_path(std::filesystem::temp_directory_path() / ("MEngineAssetTest_" + std::string(name)))
        {
            std::error_code error;
            std::filesystem::remove_all(m_path, error);
            error.clear();
            std::filesystem::create_directories(m_path, error);
            if (error)
                throw std::runtime_error("Could not create test asset directory");

            std::ofstream project(m_path / "MEngine.project");
            project << "MENGINE_PROJECT 1\n"
                       "namespace a1b64b08-8c90-40f9-8256-9c21aaa91154\n"
                       "end_project\n";
            std::ofstream registry(m_path / "AssetRegistry.mregistry");
            registry << "MENGINE_ASSET_REGISTRY 1\n"
                        "asset_count 0\n"
                        "end_registry\n";
            if (!project || !registry)
                throw std::runtime_error("Could not write test asset files");
        }

        ~TestAssetDirectory() noexcept
        {
            std::error_code ignored;
            std::filesystem::remove_all(m_path, ignored);
        }

        TestAssetDirectory(const TestAssetDirectory&) = delete;
        TestAssetDirectory& operator=(const TestAssetDirectory&) = delete;

        [[nodiscard]] const std::filesystem::path& Path() const noexcept { return m_path; }

    private:
        std::filesystem::path m_path;
    };
} // namespace GE::Tests
