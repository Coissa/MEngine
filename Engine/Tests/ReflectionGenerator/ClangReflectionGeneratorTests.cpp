#include "Clang/ClangDeclarationValidation.h"
#include "Clang/ClangHeaderScanner.h"
#include "Clang/ReflectionCodeRenderer.h"
#include "Clang/ReflectionGenerationPipeline.h"
#include "Fixtures/ClangRenderInput.h.reflection.generated.h"
#include "GeneratedFilePlan.h"
#include "GeneratedFileWriter.h"
#include "HeaderScanJobBuilder.h"
#include "HeaderScanJobValidation.h"
#include "ModuleManifestLoader.h"
#include "ValueViewFactory.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace
{
    namespace Generator = GE::Reflection::Generator;

    int failure_count = 0;

#define CHECK(expression)                                                   \
    do                                                                      \
    {                                                                       \
        if (!(expression))                                                  \
        {                                                                   \
            std::cerr << "CHECK failed: " #expression                     \
                      << " (line " << __LINE__ << ")\n";                 \
            ++failure_count;                                                \
        }                                                                   \
    } while (false)

    const Generator::ClangRecordDeclaration* FindRecord(
        const Generator::ClangHeaderScanOutput& output,
        const std::string& qualified_name)
    {
        for (const Generator::ClangRecordDeclaration& record : output.records)
        {
            if (record.qualified_name == qualified_name)
            {
                return &record;
            }
        }

        return nullptr;
    }

    std::string ReadTextFile(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return std::string{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
    }

    void WriteTextFile(
        const std::filesystem::path& path,
        const std::string& contents)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        CHECK(output.is_open());
        output.write(
            contents.data(),
            static_cast<std::streamsize>(contents.size()));
        CHECK(static_cast<bool>(output));
    }

    std::string EscapeJsonString(std::string_view value)
    {
        std::string escaped;
        for (const char character : value)
        {
            switch (character)
            {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += character;
                break;
            }
        }
        return escaped;
    }

    Generator::ClangHeaderScanResult ScanFixture()
    {
        const std::filesystem::path header_path =
            MENGINE_TEST_SCAN_FIXTURE;
        const std::filesystem::path source_root =
            MENGINE_TEST_SOURCE_ROOT;

        return Generator::ScanHeaderWithClang(
            Generator::HeaderParseInvocation{
                Generator::HeaderScanJob{
                    "ReflectionGeneratorTests",
                    source_root,
                    "Engine/Tests/ReflectionGenerator/Fixtures/ClangScanInput.h",
                    header_path,
                    source_root / "Generated",
                    source_root / "CMakeLists.txt"},
                source_root,
                std::vector<std::string>{
                    MENGINE_TEST_CLANG_EXECUTABLE,
                    "/nologo",
                    "/TP",
                    "/std:c++20",
                    "/I" + source_root.string(),
                    "/c",
                    header_path.string()}});
    }

    void PrintScanError(const Generator::ClangHeaderScanError& error)
    {
        std::cerr << "Clang scan failed with code "
                  << static_cast<int>(error.code) << '\n';
        for (const Generator::ClangDiagnostic& diagnostic : error.diagnostics)
        {
            std::cerr << diagnostic.source_file.string()
                      << ':' << diagnostic.line
                      << ':' << diagnostic.column
                      << ": " << diagnostic.message << '\n';
        }
    }

    void CheckPlanError(
        const Generator::HeaderScanJob& job,
        Generator::GeneratedFilePlanErrorCode expected_code)
    {
        const Generator::GeneratedFilePlanResult result =
            Generator::BuildGeneratedFilePlan(job);
        const auto* error =
            std::get_if<Generator::GeneratedFilePlanError>(&result);
        CHECK(error != nullptr);
        if (error != nullptr)
        {
            CHECK(error->code == expected_code);
        }
    }

    void CheckJobValidationError(
        const Generator::HeaderScanJob& job,
        Generator::HeaderScanJobValidationErrorCode expected_code)
    {
        const std::optional<Generator::HeaderScanJobValidationError> error =
            Generator::ValidateHeaderScanJob(job);
        CHECK(error.has_value());
        if (error.has_value())
        {
            CHECK(error->code == expected_code);
        }
    }

    void TestManifestPathValidation()
    {
        const Generator::ModuleManifest valid_manifest{
            "PathTests",
            ".",
            "Generated/Reflection",
            std::vector<std::filesystem::path>{"Include/Value.h"},
            "Source/Value.cpp"};
        CHECK(Generator::ValidateModuleManifest(valid_manifest) ==
              Generator::ModuleManifestValidationResult::Valid);

        Generator::ModuleManifest manifest = valid_manifest;
        manifest.module_root.clear();
        CHECK(Generator::ValidateModuleManifest(manifest) ==
              Generator::ModuleManifestValidationResult::EmptyModuleRoot);

        manifest = valid_manifest;
        manifest.module_root = "D:/AbsoluteRoot";
        CHECK(Generator::ValidateModuleManifest(manifest) ==
              Generator::ModuleManifestValidationResult::
                  ModuleRootNotRelative);

        manifest = valid_manifest;
        manifest.output_directory = "D:/Generated";
        CHECK(Generator::ValidateModuleManifest(manifest) ==
              Generator::ModuleManifestValidationResult::
                  OutputDirectoryNotRelative);

        manifest = valid_manifest;
        manifest.output_directory = "../Generated";
        CHECK(Generator::ValidateModuleManifest(manifest) ==
              Generator::ModuleManifestValidationResult::
                  OutputDirectoryContainsParentTraversal);

        manifest = valid_manifest;
        manifest.header_files = {std::filesystem::path{}};
        CHECK(Generator::ValidateModuleManifest(manifest) ==
              Generator::ModuleManifestValidationResult::EmptyHeaderFile);

        manifest = valid_manifest;
        manifest.header_files = {"D:/Absolute.h"};
        CHECK(Generator::ValidateModuleManifest(manifest) ==
              Generator::ModuleManifestValidationResult::
                  HeaderFileNotRelative);

        manifest = valid_manifest;
        manifest.header_files = {"../Outside.h"};
        CHECK(Generator::ValidateModuleManifest(manifest) ==
              Generator::ModuleManifestValidationResult::
                  HeaderFileContainsParentTraversal);

        manifest = valid_manifest;
        manifest.header_files = {"Include/Value.h", "Include/./Value.h"};
        CHECK(Generator::ValidateModuleManifest(manifest) ==
              Generator::ModuleManifestValidationResult::
                  DuplicateHeaderFile);

        manifest = valid_manifest;
        manifest.compile_command_source = "D:/Source.cpp";
        CHECK(Generator::ValidateModuleManifest(manifest) ==
              Generator::ModuleManifestValidationResult::
                  CompileCommandSourceNotRelative);

        manifest = valid_manifest;
        manifest.compile_command_source = "../Source.cpp";
        CHECK(Generator::ValidateModuleManifest(manifest) ==
              Generator::ModuleManifestValidationResult::
                  CompileCommandSourceContainsParentTraversal);
    }

    void TestManifestJobAndGeneratedFilePlan()
    {
        const std::filesystem::path source_root = MENGINE_TEST_SOURCE_ROOT;
        const std::filesystem::path manifest_path = MENGINE_TEST_MANIFEST;

        const Generator::ModuleManifestLoadResult load_result =
            Generator::LoadModuleManifest(manifest_path);
        const auto* manifest =
            std::get_if<Generator::ModuleManifest>(&load_result);
        CHECK(manifest != nullptr);
        if (manifest == nullptr)
        {
            return;
        }

        CHECK(manifest->module_root == std::filesystem::path{".."});
        CHECK(manifest->header_files.size() == 1);

        const std::vector<Generator::HeaderScanJob> jobs =
            Generator::BuildHeaderScanJobs(
                *manifest,
                manifest_path.parent_path());
        CHECK(jobs.size() == 1);
        if (jobs.size() != 1)
        {
            return;
        }

        const Generator::HeaderScanJob& job = jobs[0];
        const std::filesystem::path expected_module_root =
            (source_root / "Engine").lexically_normal();
        const std::filesystem::path expected_relative_header =
            "Engine/Core/Reflection/TypeRegistry.h";
        const std::filesystem::path expected_header =
            (expected_module_root / expected_relative_header)
                .lexically_normal();
        const std::filesystem::path expected_output =
            (expected_module_root / "Generated/Reflection")
                .lexically_normal();

        CHECK(job.module_root == expected_module_root);
        CHECK(job.module_relative_header_path == expected_relative_header);
        CHECK(job.header_path == expected_header);
        CHECK(job.output_directory == expected_output);
        CHECK(!Generator::ValidateHeaderScanJob(job).has_value());

        const Generator::GeneratedFilePlanResult plan_result =
            Generator::BuildGeneratedFilePlan(job);
        const auto* plan =
            std::get_if<Generator::GeneratedFilePlan>(&plan_result);
        CHECK(plan != nullptr);
        if (plan == nullptr)
        {
            return;
        }

        CHECK(plan->source_header_path == expected_header);
        CHECK(plan->generated_header_path ==
              expected_output /
                  "Engine/Core/Reflection/TypeRegistry.h.reflection.generated.h");
        CHECK(plan->generated_source_path ==
              expected_output /
                  "Engine/Core/Reflection/TypeRegistry.h.reflection.generated.cpp");

        Generator::HeaderScanJob invalid_job = job;
        invalid_job.module_root = "relative/root";
        CheckPlanError(
            invalid_job,
            Generator::GeneratedFilePlanErrorCode::ModuleRootNotAbsolute);
        CheckJobValidationError(
            invalid_job,
            Generator::HeaderScanJobValidationErrorCode::
                ModuleRootNotAbsolute);

        invalid_job = job;
        invalid_job.module_root = source_root / "MissingModuleRoot";
        CheckJobValidationError(
            invalid_job,
            Generator::HeaderScanJobValidationErrorCode::ModuleRootNotFound);

        invalid_job = job;
        invalid_job.module_root = source_root / "CMakeLists.txt";
        CheckJobValidationError(
            invalid_job,
            Generator::HeaderScanJobValidationErrorCode::
                ModuleRootNotDirectory);

        invalid_job = job;
        invalid_job.module_relative_header_path.clear();
        CheckPlanError(
            invalid_job,
            Generator::GeneratedFilePlanErrorCode::
                EmptyModuleRelativeHeaderPath);
        CheckJobValidationError(
            invalid_job,
            Generator::HeaderScanJobValidationErrorCode::
                EmptyModuleRelativeHeaderPath);

        invalid_job = job;
        invalid_job.module_relative_header_path = "D:/Absolute.h";
        CheckPlanError(
            invalid_job,
            Generator::GeneratedFilePlanErrorCode::
                ModuleRelativeHeaderPathNotRelative);
        CheckJobValidationError(
            invalid_job,
            Generator::HeaderScanJobValidationErrorCode::
                ModuleRelativeHeaderPathNotRelative);

        invalid_job = job;
        invalid_job.module_relative_header_path = "../Outside.h";
        CheckPlanError(
            invalid_job,
            Generator::GeneratedFilePlanErrorCode::
                ModuleRelativeHeaderPathContainsParentTraversal);
        CheckJobValidationError(
            invalid_job,
            Generator::HeaderScanJobValidationErrorCode::
                ModuleRelativeHeaderPathContainsParentTraversal);

        invalid_job = job;
        invalid_job.module_relative_header_path = ".";
        CheckPlanError(
            invalid_job,
            Generator::GeneratedFilePlanErrorCode::MissingHeaderFileName);

        invalid_job = job;
        invalid_job.header_path = source_root / "CMakeLists.txt";
        CheckPlanError(
            invalid_job,
            Generator::GeneratedFilePlanErrorCode::
                HeaderPathDoesNotMatchModuleRelativePath);
        CheckJobValidationError(
            invalid_job,
            Generator::HeaderScanJobValidationErrorCode::
                HeaderPathDoesNotMatchModuleRelativePath);

        invalid_job = job;
        invalid_job.header_path = "relative/TypeRegistry.h";
        CheckPlanError(
            invalid_job,
            Generator::GeneratedFilePlanErrorCode::HeaderPathNotAbsolute);
        CheckJobValidationError(
            invalid_job,
            Generator::HeaderScanJobValidationErrorCode::
                HeaderPathNotAbsolute);

        invalid_job = job;
        invalid_job.output_directory = "relative/output";
        CheckPlanError(
            invalid_job,
            Generator::GeneratedFilePlanErrorCode::OutputDirectoryNotAbsolute);
        CheckJobValidationError(
            invalid_job,
            Generator::HeaderScanJobValidationErrorCode::
                OutputDirectoryNotAbsolute);
    }

    void TestReflectionCodeRendering()
    {
        const std::filesystem::path source_root = MENGINE_TEST_SOURCE_ROOT;
        const std::filesystem::path header_path = MENGINE_TEST_RENDER_FIXTURE;
        const std::filesystem::path fixture_directory =
            header_path.parent_path();
        const Generator::HeaderScanJob job{
            "ReflectionRendererTests",
            fixture_directory,
            "ClangRenderInput.h",
            header_path,
            fixture_directory,
            source_root / "CMakeLists.txt"};

        const Generator::ClangHeaderScanResult scan_result =
            Generator::ScanHeaderWithClang(
                Generator::HeaderParseInvocation{
                    job,
                    source_root,
                    std::vector<std::string>{
                        MENGINE_TEST_CLANG_EXECUTABLE,
                        "/nologo",
                        "/TP",
                        "/std:c++20",
                        "/I" + source_root.string(),
                        "/c",
                        header_path.string()}});
        const auto* scan_output =
            std::get_if<Generator::ClangHeaderScanOutput>(&scan_result);
        if (scan_output == nullptr)
        {
            PrintScanError(
                std::get<Generator::ClangHeaderScanError>(scan_result));
            CHECK(false);
            return;
        }

        const Generator::GeneratedFilePlanResult plan_result =
            Generator::BuildGeneratedFilePlan(job);
        const auto* file_plan =
            std::get_if<Generator::GeneratedFilePlan>(&plan_result);
        CHECK(file_plan != nullptr);
        if (file_plan == nullptr)
        {
            return;
        }

        const Generator::ReflectionCodeRenderResult render_result =
            Generator::RenderReflectionCode(job, *scan_output, *file_plan);
        const auto* rendered =
            std::get_if<Generator::ReflectionCodeRenderOutput>(&render_result);
        CHECK(rendered != nullptr);
        if (rendered == nullptr)
        {
            return;
        }

        CHECK(rendered->registration_function_name ==
              "RegisterGeneratedReflection_f57f7e125e8196b3");

        const std::string golden_header =
            ReadTextFile(MENGINE_TEST_RENDER_GOLDEN_HEADER);
        const std::string golden_source =
            ReadTextFile(MENGINE_TEST_RENDER_GOLDEN_SOURCE);
        if (rendered->header_file.contents != golden_header)
        {
            std::cerr << "Rendered header did not match golden file.\n"
                      << rendered->header_file.contents << '\n';
            CHECK(false);
        }
        if (rendered->source_file.contents != golden_source)
        {
            std::cerr << "Rendered source did not match golden file.\n"
                      << rendered->source_file.contents << '\n';
            CHECK(false);
        }

        Generator::ClangHeaderScanOutput invalid_output = *scan_output;
        invalid_output.records[0].fields[0].is_bit_field = true;
        const Generator::ReflectionCodeRenderResult invalid_render_result =
            Generator::RenderReflectionCode(
                job,
                invalid_output,
                *file_plan);
        const auto* render_error =
            std::get_if<Generator::ReflectionCodeRenderError>(
                &invalid_render_result);
        CHECK(render_error != nullptr);
        if (render_error != nullptr)
        {
            CHECK(render_error->code ==
                  Generator::ReflectionCodeRenderErrorCode::
                      DeclarationValidationFailed);
            CHECK(render_error->declaration_errors.size() == 1);
        }
    }

    void TestCompiledGeneratedReflection()
    {
        using GE::Reflection::ConstValueView;
        using GE::Reflection::FieldInfo;
        using GE::Reflection::MakeConstValueView;
        using GE::Reflection::MakeValueView;
        using GE::Reflection::TypeInfo;
        using GE::Reflection::TypeRegistry;
        using GE::Reflection::ValueView;

        TypeRegistry registry;
        CHECK(GE::Reflection::
                  RegisterGeneratedReflection_f57f7e125e8196b3(registry) ==
              TypeRegistry::RegisterTypeResult::Success);

        const TypeInfo* type_info = registry.FindType(
            "ReflectionRendererTests::RenderSample");
        CHECK(type_info != nullptr);
        if (type_info == nullptr)
        {
            return;
        }

        const std::span<const FieldInfo> fields = type_info->GetFields();
        CHECK(fields.size() == 2);
        if (fields.size() != 2)
        {
            return;
        }

        ReflectionRendererTests::RenderSample sample;
        ValueView mutable_value = fields[0].GetValue(MakeValueView(sample));
        CHECK(mutable_value.IsValid());
        if (mutable_value.IsValid())
        {
            CHECK(*static_cast<int*>(mutable_value.Data()) == 42);
            *static_cast<int*>(mutable_value.Data()) = 91;
            CHECK(sample.GetValue() == 91);
        }

        CHECK(fields[1].IsReadOnly());
        CHECK(!fields[1].GetValue(MakeValueView(sample)).IsValid());
        const ConstValueView const_weight =
            fields[1].GetValue(MakeConstValueView(sample));
        CHECK(const_weight.IsValid());
        if (const_weight.IsValid())
        {
            CHECK(*static_cast<const float*>(const_weight.Data()) == 1.5F);
        }
    }

    void TestGeneratedFileWriter()
    {
        const std::filesystem::path test_directory =
            std::filesystem::path{MENGINE_TEST_BINARY_DIR} /
            "GeneratedFileWriterTests";
        std::error_code filesystem_error;
        std::filesystem::remove_all(test_directory, filesystem_error);
        CHECK(!filesystem_error);

        const std::filesystem::path header_path =
            test_directory / "Nested/Sample.reflection.generated.h";
        const std::filesystem::path source_path =
            test_directory / "Nested/Sample.reflection.generated.cpp";
        std::vector<Generator::GeneratedCodeFile> files{
            Generator::GeneratedCodeFile{header_path, "header-v1\n"},
            Generator::GeneratedCodeFile{source_path, "source-v1\n"}};

        Generator::GeneratedFilesWriteResult first_result =
            Generator::WriteGeneratedCodeFiles(files);
        const auto* first_output =
            std::get_if<Generator::GeneratedFilesWriteOutput>(&first_result);
        CHECK(first_output != nullptr);
        if (first_output != nullptr)
        {
            CHECK(first_output->files.size() == 2);
            CHECK(first_output->files[0].disposition ==
                  Generator::GeneratedFileWriteDisposition::Written);
            CHECK(first_output->files[1].disposition ==
                  Generator::GeneratedFileWriteDisposition::Written);
        }
        CHECK(ReadTextFile(header_path) == "header-v1\n");
        CHECK(ReadTextFile(source_path) == "source-v1\n");

        Generator::GeneratedFilesWriteResult unchanged_result =
            Generator::WriteGeneratedCodeFiles(files);
        const auto* unchanged_output =
            std::get_if<Generator::GeneratedFilesWriteOutput>(
                &unchanged_result);
        CHECK(unchanged_output != nullptr);
        if (unchanged_output != nullptr)
        {
            CHECK(unchanged_output->files.size() == 2);
            CHECK(unchanged_output->files[0].disposition ==
                  Generator::GeneratedFileWriteDisposition::Unchanged);
            CHECK(unchanged_output->files[1].disposition ==
                  Generator::GeneratedFileWriteDisposition::Unchanged);
        }

        files[0].contents = "header-v2\n";
        Generator::GeneratedFilesWriteResult update_result =
            Generator::WriteGeneratedCodeFiles(files);
        const auto* update_output =
            std::get_if<Generator::GeneratedFilesWriteOutput>(&update_result);
        CHECK(update_output != nullptr);
        if (update_output != nullptr)
        {
            CHECK(update_output->files[0].disposition ==
                  Generator::GeneratedFileWriteDisposition::Written);
            CHECK(update_output->files[1].disposition ==
                  Generator::GeneratedFileWriteDisposition::Unchanged);
        }
        CHECK(ReadTextFile(header_path) == "header-v2\n");
        CHECK(ReadTextFile(source_path) == "source-v1\n");

        const std::vector<Generator::GeneratedCodeFile> empty_path_file{
            Generator::GeneratedCodeFile{{}, "text"}};
        const Generator::GeneratedFilesWriteResult empty_path_result =
            Generator::WriteGeneratedCodeFiles(empty_path_file);
        const auto* empty_path_error =
            std::get_if<Generator::GeneratedFileWriteError>(
                &empty_path_result);
        CHECK(empty_path_error != nullptr);
        if (empty_path_error != nullptr)
        {
            CHECK(empty_path_error->code ==
                  Generator::GeneratedFileWriteErrorCode::EmptyPath);
        }

        const std::vector<Generator::GeneratedCodeFile> relative_file{
            Generator::GeneratedCodeFile{"relative.generated.h", "text"}};
        const Generator::GeneratedFilesWriteResult relative_result =
            Generator::WriteGeneratedCodeFiles(relative_file);
        const auto* relative_error =
            std::get_if<Generator::GeneratedFileWriteError>(
                &relative_result);
        CHECK(relative_error != nullptr);
        if (relative_error != nullptr)
        {
            CHECK(relative_error->code ==
                  Generator::GeneratedFileWriteErrorCode::PathNotAbsolute);
        }

        const std::vector<Generator::GeneratedCodeFile> duplicate_files{
            Generator::GeneratedCodeFile{header_path, "first"},
            Generator::GeneratedCodeFile{header_path, "second"}};
        const Generator::GeneratedFilesWriteResult duplicate_result =
            Generator::WriteGeneratedCodeFiles(duplicate_files);
        const auto* duplicate_error =
            std::get_if<Generator::GeneratedFileWriteError>(
                &duplicate_result);
        CHECK(duplicate_error != nullptr);
        if (duplicate_error != nullptr)
        {
            CHECK(duplicate_error->code ==
                  Generator::GeneratedFileWriteErrorCode::
                      DuplicateOutputPath);
        }
        CHECK(ReadTextFile(header_path) == "header-v2\n");

        const std::filesystem::path blocked_parent =
            test_directory / "BlockedParent";
        {
            std::ofstream output(blocked_parent, std::ios::binary);
            output << "not a directory";
        }
        const std::vector<Generator::GeneratedCodeFile> blocked_file{
            Generator::GeneratedCodeFile{
                blocked_parent / "Generated.h",
                "text"}};
        const Generator::GeneratedFilesWriteResult blocked_result =
            Generator::WriteGeneratedCodeFiles(blocked_file);
        const auto* blocked_error =
            std::get_if<Generator::GeneratedFileWriteError>(
                &blocked_result);
        CHECK(blocked_error != nullptr);
        if (blocked_error != nullptr)
        {
            CHECK(blocked_error->code ==
                  Generator::GeneratedFileWriteErrorCode::
                      CreateParentDirectoryFailed);
            CHECK(static_cast<bool>(blocked_error->filesystem_error));
        }

        const std::filesystem::path directory_destination =
            test_directory / "DirectoryDestination";
        std::filesystem::create_directories(
            directory_destination,
            filesystem_error);
        CHECK(!filesystem_error);
        const std::vector<Generator::GeneratedCodeFile> directory_file{
            Generator::GeneratedCodeFile{directory_destination, "text"}};
        const Generator::GeneratedFilesWriteResult directory_result =
            Generator::WriteGeneratedCodeFiles(directory_file);
        const auto* directory_error =
            std::get_if<Generator::GeneratedFileWriteError>(
                &directory_result);
        CHECK(directory_error != nullptr);
        if (directory_error != nullptr)
        {
            CHECK(directory_error->code ==
                  Generator::GeneratedFileWriteErrorCode::
                      DestinationNotRegularFile);
        }

        std::size_t temporary_file_count = 0;
        for (const std::filesystem::directory_entry& entry :
             std::filesystem::recursive_directory_iterator(test_directory))
        {
            if (entry.path().filename().string().find(".tmp.")
                != std::string::npos)
            {
                ++temporary_file_count;
            }
        }
        CHECK(temporary_file_count == 0);

        filesystem_error.clear();
        std::filesystem::remove_all(test_directory, filesystem_error);
        CHECK(!filesystem_error);
    }

    void TestReflectionGenerationPipeline()
    {
        const std::filesystem::path source_root = MENGINE_TEST_SOURCE_ROOT;
        const std::filesystem::path test_directory =
            std::filesystem::path{MENGINE_TEST_BINARY_DIR} /
            "ReflectionGenerationPipelineTests";
        const std::filesystem::path module_directory =
            test_directory / "Module";
        const std::filesystem::path header_path =
            module_directory / "Input.h";
        const std::filesystem::path anchor_path =
            module_directory / "Anchor.cpp";
        const std::filesystem::path manifest_path =
            test_directory / "Pipeline.Reflection.json";
        const std::filesystem::path compilation_database_path =
            test_directory / "compile_commands.json";
        const std::filesystem::path generated_header_path =
            module_directory /
            "Generated/Input.h.reflection.generated.h";
        const std::filesystem::path generated_source_path =
            module_directory /
            "Generated/Input.h.reflection.generated.cpp";
        const std::filesystem::path generated_module_header_path =
            module_directory /
            "Generated/PipelineTests.reflection.module.generated.h";
        const std::filesystem::path generated_module_source_path =
            module_directory /
            "Generated/PipelineTests.reflection.module.generated.cpp";

        std::error_code filesystem_error;
        std::filesystem::remove_all(test_directory, filesystem_error);
        CHECK(!filesystem_error);
        std::filesystem::create_directories(
            module_directory,
            filesystem_error);
        CHECK(!filesystem_error);

        WriteTextFile(
            header_path,
            "#pragma once\n\n"
            "#include \"Engine/Engine/Core/Reflection/ReflectionMacros.h\"\n\n"
            "namespace PipelineTests\n"
            "{\n"
            "    class MENGINE_REFLECT_TYPE PipelineType\n"
            "    {\n"
            "        MENGINE_REFLECT_BODY(PipelineType)\n\n"
            "    private:\n"
            "        MENGINE_REFLECT_FIELD int value_ {7};\n"
            "    };\n"
            "}\n");
        WriteTextFile(anchor_path, "// compile command anchor\n");
        WriteTextFile(
            manifest_path,
            "{\n"
            "  \"module_name\": \"PipelineTests\",\n"
            "  \"module_root\": \"Module\",\n"
            "  \"header_files\": [\"Input.h\"],\n"
            "  \"output_directory\": \"Generated\",\n"
            "  \"compile_command_source\": \"Anchor.cpp\"\n"
            "}\n");

        const std::string clang_executable = EscapeJsonString(
            std::filesystem::path{MENGINE_TEST_CLANG_EXECUTABLE}
                .generic_string());
        const std::string working_directory = EscapeJsonString(
            source_root.generic_string());
        const std::string anchor_argument = EscapeJsonString(
            anchor_path.generic_string());
        const std::string include_argument = EscapeJsonString(
            "/I" + source_root.generic_string());
        WriteTextFile(
            compilation_database_path,
            "[\n"
            "  {\n"
            "    \"directory\": \"" + working_directory + "\",\n"
            "    \"file\": \"" + anchor_argument + "\",\n"
            "    \"arguments\": [\n"
            "      \"" + clang_executable + "\",\n"
            "      \"/nologo\",\n"
            "      \"/TP\",\n"
            "      \"/std:c++20\",\n"
            "      \"" + include_argument + "\",\n"
            "      \"/c\",\n"
            "      \"" + anchor_argument + "\"\n"
            "    ]\n"
            "  }\n"
            "]\n");

        const Generator::ReflectionGenerationRequest request{
            compilation_database_path,
            std::vector<std::filesystem::path>{manifest_path}};
        Generator::ReflectionGenerationResult first_result =
            Generator::RunReflectionGeneration(request);
        const auto* first_output =
            std::get_if<Generator::ReflectionGenerationOutput>(
                &first_result);
        CHECK(first_output != nullptr);
        if (first_output != nullptr)
        {
            CHECK(first_output->module_count == 1);
            CHECK(first_output->header_count == 1);
            CHECK(first_output->reflected_type_count == 1);
            CHECK(first_output->files.size() == 4);
            CHECK(first_output->files[0].disposition ==
                  Generator::GeneratedFileWriteDisposition::Written);
            CHECK(first_output->files[1].disposition ==
                  Generator::GeneratedFileWriteDisposition::Written);
            CHECK(first_output->files[2].disposition ==
                  Generator::GeneratedFileWriteDisposition::Written);
            CHECK(first_output->files[3].disposition ==
                  Generator::GeneratedFileWriteDisposition::Written);
        }

        CHECK(std::filesystem::is_regular_file(generated_header_path));
        CHECK(std::filesystem::is_regular_file(generated_source_path));
        CHECK(std::filesystem::is_regular_file(
            generated_module_header_path));
        CHECK(std::filesystem::is_regular_file(
            generated_module_source_path));
        CHECK(ReadTextFile(generated_header_path).find(
                  "PipelineTests::PipelineType") != std::string::npos);
        CHECK(ReadTextFile(generated_source_path).find(
                  "RegisterGeneratedReflection_") != std::string::npos);
        CHECK(ReadTextFile(generated_module_source_path).find(
                  "RegisterGeneratedReflectionModule_")
              != std::string::npos);
        CHECK(ReadTextFile(generated_module_source_path).find(
                  "RegisterGeneratedReflection_")
              != std::string::npos);

        Generator::ReflectionGenerationResult second_result =
            Generator::RunReflectionGeneration(request);
        const auto* second_output =
            std::get_if<Generator::ReflectionGenerationOutput>(
                &second_result);
        CHECK(second_output != nullptr);
        if (second_output != nullptr)
        {
            CHECK(second_output->files.size() == 4);
            CHECK(second_output->files[0].disposition ==
                  Generator::GeneratedFileWriteDisposition::Unchanged);
            CHECK(second_output->files[1].disposition ==
                  Generator::GeneratedFileWriteDisposition::Unchanged);
            CHECK(second_output->files[2].disposition ==
                  Generator::GeneratedFileWriteDisposition::Unchanged);
            CHECK(second_output->files[3].disposition ==
                  Generator::GeneratedFileWriteDisposition::Unchanged);
        }

        const std::filesystem::path invalid_header_path =
            module_directory / "Invalid.h";
        WriteTextFile(
            invalid_header_path,
            "#pragma once\n\n"
            "#include \"Engine/Engine/Core/Reflection/ReflectionMacros.h\"\n\n"
            "namespace PipelineTests\n"
            "{\n"
            "    class MENGINE_REFLECT_TYPE InvalidType\n"
            "    {\n"
            "        MENGINE_REFLECT_BODY(InvalidType)\n\n"
            "    private:\n"
            "        MENGINE_REFLECT_FIELD unsigned int flags_ : 1;\n"
            "    };\n"
            "}\n");
        WriteTextFile(
            manifest_path,
            "{\n"
            "  \"module_name\": \"PipelineTests\",\n"
            "  \"module_root\": \"Module\",\n"
            "  \"header_files\": [\"Input.h\", \"Invalid.h\"],\n"
            "  \"output_directory\": \"Generated\",\n"
            "  \"compile_command_source\": \"Anchor.cpp\"\n"
            "}\n");
        filesystem_error.clear();
        std::filesystem::remove_all(
            module_directory / "Generated",
            filesystem_error);
        CHECK(!filesystem_error);

        const Generator::ReflectionGenerationResult render_failure_result =
            Generator::RunReflectionGeneration(request);
        const auto* render_failure =
            std::get_if<Generator::ReflectionGenerationError>(
                &render_failure_result);
        CHECK(render_failure != nullptr);
        if (render_failure != nullptr)
        {
            CHECK(render_failure->code ==
                  Generator::ReflectionGenerationErrorCode::
                      CodeRenderFailed);
            CHECK(render_failure->job_index == 1);
            CHECK(render_failure->render_error.has_value());
            if (render_failure->render_error.has_value())
            {
                CHECK(render_failure->render_error
                          ->declaration_errors.size() == 1);
            }
        }
        CHECK(!std::filesystem::exists(generated_header_path));
        CHECK(!std::filesystem::exists(generated_source_path));
        CHECK(!std::filesystem::exists(generated_module_header_path));
        CHECK(!std::filesystem::exists(generated_module_source_path));

        Generator::ReflectionGenerationRequest invalid_request = request;
        invalid_request.manifest_paths[0] = "relative-manifest.json";
        const Generator::ReflectionGenerationResult invalid_result =
            Generator::RunReflectionGeneration(invalid_request);
        const auto* invalid_error =
            std::get_if<Generator::ReflectionGenerationError>(
                &invalid_result);
        CHECK(invalid_error != nullptr);
        if (invalid_error != nullptr)
        {
            CHECK(invalid_error->code ==
                  Generator::ReflectionGenerationErrorCode::
                      ManifestPathNotAbsolute);
        }

        filesystem_error.clear();
        std::filesystem::remove_all(test_directory, filesystem_error);
        CHECK(!filesystem_error);
    }

    void TestClangScanner()
    {
        const Generator::ClangHeaderScanResult result = ScanFixture();
        const auto* output =
            std::get_if<Generator::ClangHeaderScanOutput>(&result);
        if (output == nullptr)
        {
            PrintScanError(std::get<Generator::ClangHeaderScanError>(result));
            CHECK(false);
            return;
        }

        CHECK(output->module_name == "ReflectionGeneratorTests");
        CHECK(output->records.size() == 2);
        CHECK(FindRecord(
            *output,
            "ReflectionGeneratorTests::IncludedReflectedType") == nullptr);
        CHECK(FindRecord(
            *output,
            "ReflectionGeneratorTests::NotReflected") == nullptr);
        CHECK(FindRecord(
            *output,
            "ReflectionGeneratorTests::OtherAnnotation") == nullptr);

        const Generator::ClangRecordDeclaration* reflected_class =
            FindRecord(
                *output,
                "ReflectionGeneratorTests::ReflectedClass");
        CHECK(reflected_class != nullptr);
        if (reflected_class == nullptr)
        {
            return;
        }

        CHECK(reflected_class->kind ==
              Generator::ClangRecordDeclarationKind::Class);
        CHECK(reflected_class->fields.size() == 3);
        if (reflected_class->fields.size() != 3)
        {
            return;
        }

        const Generator::ClangFieldDeclaration& public_value =
            reflected_class->fields[0];
        CHECK(public_value.name == "public_value");
        CHECK(public_value.type_spelling == "int");
        CHECK(public_value.access == Generator::ClangFieldAccess::Public);
        CHECK(!public_value.is_const);
        CHECK(!public_value.is_bit_field);

        const Generator::ClangFieldDeclaration& fixed_value =
            reflected_class->fields[1];
        CHECK(fixed_value.name == "fixed_value");
        CHECK(fixed_value.type_spelling == "const int");
        CHECK(fixed_value.access == Generator::ClangFieldAccess::Protected);
        CHECK(fixed_value.is_const);
        CHECK(!fixed_value.is_bit_field);

        const Generator::ClangFieldDeclaration& flags =
            reflected_class->fields[2];
        CHECK(flags.name == "flags");
        CHECK(flags.type_spelling == "unsigned int");
        CHECK(flags.access == Generator::ClangFieldAccess::Private);
        CHECK(!flags.is_const);
        CHECK(flags.is_bit_field);

        const Generator::ClangRecordDeclaration* reflected_struct =
            FindRecord(
                *output,
                "ReflectionGeneratorTests::EmptyReflectedStruct");
        CHECK(reflected_struct != nullptr);
        if (reflected_struct != nullptr)
        {
            CHECK(reflected_struct->kind ==
                  Generator::ClangRecordDeclarationKind::Struct);
            CHECK(reflected_struct->fields.empty());
        }

        const Generator::ClangDeclarationValidationResult errors =
            Generator::ValidateClangDeclarations(*output);
        CHECK(errors.size() == 1);
        if (errors.size() == 1)
        {
            CHECK(errors[0].code ==
                  Generator::ClangDeclarationValidationErrorCode::
                      UnsupportedBitField);
            CHECK(errors[0].record_qualified_name ==
                  "ReflectionGeneratorTests::ReflectedClass");
            CHECK(errors[0].field_name == "flags");
        }
    }

    void TestDeclarationValidationCollectsAllErrors()
    {
        const Generator::ClangHeaderScanOutput output{
            "ValidationTests",
            "D:/Tests/Invalid.h",
            std::vector<Generator::ClangRecordDeclaration>{
                Generator::ClangRecordDeclaration{
                    Generator::ClangRecordDeclarationKind::Class,
                    {},
                    "D:/Tests/Invalid.h",
                    10,
                    3,
                    std::vector<Generator::ClangFieldDeclaration>{
                        Generator::ClangFieldDeclaration{
                            {},
                            {},
                            "D:/Tests/Invalid.h",
                            12,
                            9,
                            Generator::ClangFieldAccess::Unspecified,
                            false,
                            true},
                        Generator::ClangFieldDeclaration{
                            "value",
                            "int",
                            "D:/Tests/Invalid.h",
                            13,
                            9,
                            Generator::ClangFieldAccess::Private,
                            false,
                            false},
                        Generator::ClangFieldDeclaration{
                            "value",
                            "float",
                            "D:/Tests/Invalid.h",
                            14,
                            9,
                            Generator::ClangFieldAccess::Private,
                            false,
                            false}}}},
            {}};

        const Generator::ClangDeclarationValidationResult errors =
            Generator::ValidateClangDeclarations(output);
        CHECK(errors.size() == 6);
        if (errors.size() != 6)
        {
            return;
        }

        const Generator::ClangDeclarationValidationErrorCode expected_codes[]{
            Generator::ClangDeclarationValidationErrorCode::
                EmptyRecordQualifiedName,
            Generator::ClangDeclarationValidationErrorCode::EmptyFieldName,
            Generator::ClangDeclarationValidationErrorCode::
                EmptyFieldTypeSpelling,
            Generator::ClangDeclarationValidationErrorCode::UnsupportedBitField,
            Generator::ClangDeclarationValidationErrorCode::
                UnspecifiedFieldAccess,
            Generator::ClangDeclarationValidationErrorCode::DuplicateFieldName};

        for (std::size_t index = 0; index < errors.size(); ++index)
        {
            CHECK(errors[index].code == expected_codes[index]);
            CHECK(errors[index].module_name == "ValidationTests");
            CHECK(errors[index].header_path ==
                  std::filesystem::path{"D:/Tests/Invalid.h"});
        }

        CHECK(errors[1].source_file ==
              std::filesystem::path{"D:/Tests/Invalid.h"});
        CHECK(errors[1].line == 12);
        CHECK(errors[1].column == 9);
        CHECK(errors[5].field_name == "value");
        CHECK(errors[5].line == 14);
    }

    void TestValidDeclarationsPassValidation()
    {
        const Generator::ClangHeaderScanOutput output{
            "ValidationTests",
            "D:/Tests/Valid.h",
            std::vector<Generator::ClangRecordDeclaration>{
                Generator::ClangRecordDeclaration{
                    Generator::ClangRecordDeclarationKind::Struct,
                    "Tests::Valid",
                    "D:/Tests/Valid.h",
                    1,
                    1,
                    std::vector<Generator::ClangFieldDeclaration>{
                        Generator::ClangFieldDeclaration{
                            "value",
                            "int",
                            "D:/Tests/Valid.h",
                            3,
                            5,
                            Generator::ClangFieldAccess::Public,
                            false,
                            false}}}},
            {}};

        CHECK(Generator::ValidateClangDeclarations(output).empty());
    }

#undef CHECK
}

int main()
{
    TestManifestPathValidation();
    TestManifestJobAndGeneratedFilePlan();
    TestReflectionCodeRendering();
    TestCompiledGeneratedReflection();
    TestGeneratedFileWriter();
    TestReflectionGenerationPipeline();
    TestClangScanner();
    TestDeclarationValidationCollectsAllErrors();
    TestValidDeclarationsPassValidation();

    if (failure_count != 0)
    {
        std::cerr << failure_count
                  << " reflection generator test(s) failed.\n";
        return 1;
    }

    std::cout << "All reflection generator tests passed.\n";
    return 0;
}
