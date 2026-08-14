#include "Clang/ReflectionGenerationPipeline.h"

#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace
{
    namespace Generator = GE::Reflection::Generator;

    struct CommandLineOptions
    {
        std::filesystem::path compilation_database_path;
        std::vector<std::filesystem::path> manifest_paths;
        bool show_help = false;
    };

    struct CommandLineError
    {
        std::string message;
    };

    void PrintUsage(std::ostream& output)
    {
        output
            << "Usage:\n"
            << "  MEngineReflectionGenerator "
            << "--compile-commands <compile_commands.json> "
            << "--manifest <module.json> [--manifest <module.json> ...]\n";
    }

    std::optional<std::filesystem::path> MakeAbsolutePath(
        std::string_view value,
        std::error_code& filesystem_error)
    {
        std::filesystem::path path = std::filesystem::u8path(value);
        if (path.is_relative())
        {
            path = std::filesystem::absolute(path, filesystem_error);
            if (filesystem_error)
            {
                return std::nullopt;
            }
        }

        filesystem_error.clear();
        return path.lexically_normal();
    }

    std::variant<CommandLineOptions, CommandLineError> ParseCommandLine(
        int argument_count,
        char* arguments[])
    {
        CommandLineOptions options;

        for (int index = 1; index < argument_count; ++index)
        {
            const std::string_view argument = arguments[index];
            if (argument == "--help" || argument == "-h")
            {
                options.show_help = true;
                continue;
            }

            if (argument != "--compile-commands"
                && argument != "--manifest")
            {
                return CommandLineError{
                    "Unknown argument: " + std::string{argument}};
            }

            if (index + 1 >= argument_count)
            {
                return CommandLineError{
                    "Missing value after " + std::string{argument}};
            }

            std::error_code filesystem_error;
            const std::optional<std::filesystem::path> path =
                MakeAbsolutePath(arguments[++index], filesystem_error);
            if (!path.has_value())
            {
                return CommandLineError{
                    "Cannot make path absolute: "
                    + filesystem_error.message()};
            }

            if (argument == "--compile-commands")
            {
                if (!options.compilation_database_path.empty())
                {
                    return CommandLineError{
                        "--compile-commands may only be specified once"};
                }
                options.compilation_database_path = *path;
            }
            else
            {
                options.manifest_paths.push_back(*path);
            }
        }

        if (options.show_help)
        {
            return options;
        }
        if (options.compilation_database_path.empty())
        {
            return CommandLineError{
                "--compile-commands is required"};
        }
        if (options.manifest_paths.empty())
        {
            return CommandLineError{
                "At least one --manifest is required"};
        }

        return options;
    }

    const char* ErrorStageName(
        Generator::ReflectionGenerationErrorCode code)
    {
        using enum Generator::ReflectionGenerationErrorCode;
        switch (code)
        {
        case EmptyManifestPaths:
            return "no module manifests were supplied";
        case CompilationDatabasePathNotAbsolute:
            return "the compilation database path is not absolute";
        case ManifestPathNotAbsolute:
            return "a module manifest path is not absolute";
        case ManifestLoadFailed:
            return "module manifest loading failed";
        case GenerationPlanBuildFailed:
            return "generation plan construction failed";
        case CompilationDatabaseLoadFailed:
            return "compilation database loading failed";
        case HeaderScanJobResolutionFailed:
            return "compile command lookup failed";
        case HeaderParseInvocationBuildFailed:
            return "Clang invocation construction failed";
        case GeneratedFilePlanningFailed:
            return "generated file planning failed";
        case HeaderScanFailed:
            return "Clang header scanning failed";
        case CodeRenderFailed:
            return "reflection code rendering failed";
        case ModuleCodeRenderFailed:
            return "module registration rendering failed";
        case FileWriteFailed:
            return "generated file output failed";
        }
        return "unknown stage failed";
    }

    const char* DiagnosticSeverityName(
        Generator::ClangDiagnosticSeverity severity)
    {
        using enum Generator::ClangDiagnosticSeverity;
        switch (severity)
        {
        case Note:
            return "note";
        case Warning:
            return "warning";
        case Error:
            return "error";
        case Fatal:
            return "fatal";
        }
        return "diagnostic";
    }

    void PrintDiagnostic(const Generator::ClangDiagnostic& diagnostic)
    {
        if (!diagnostic.source_file.empty())
        {
            std::cerr << diagnostic.source_file.string();
            if (diagnostic.line != 0)
            {
                std::cerr << ':' << diagnostic.line;
                if (diagnostic.column != 0)
                {
                    std::cerr << ':' << diagnostic.column;
                }
            }
            std::cerr << ": ";
        }
        std::cerr << DiagnosticSeverityName(diagnostic.severity)
                  << ": " << diagnostic.message << '\n';
    }

    void PrintGenerationError(
        const Generator::ReflectionGenerationError& error)
    {
        std::cerr << "Reflection generation failed: "
                  << ErrorStageName(error.code) << '\n';
        if (error.job_index !=
            Generator::ReflectionGenerationError::NoJobIndex)
        {
            std::cerr << "  job index: " << error.job_index << '\n';
        }
        if (!error.module_name.empty())
        {
            std::cerr << "  module: " << error.module_name << '\n';
        }
        if (!error.path.empty())
        {
            std::cerr << "  path: " << error.path.string() << '\n';
        }

        if (error.manifest_load_error.has_value())
        {
            std::cerr << "  detail: "
                      << error.manifest_load_error->error_message << '\n';
        }
        if (error.compilation_database_error.has_value())
        {
            std::cerr << "  detail: "
                      << error.compilation_database_error->message << '\n';
            if (error.compilation_database_error->entry_index
                != std::numeric_limits<std::size_t>::max())
            {
                std::cerr << "  database entry: "
                          << error.compilation_database_error->entry_index
                          << '\n';
            }
        }
        if (error.plan_build_error.has_value()
            && error.plan_build_error->scan_job_error.has_value()
            && error.plan_build_error->scan_job_error->filesystem_error)
        {
            std::cerr << "  filesystem: "
                      << error.plan_build_error->scan_job_error
                             ->filesystem_error.message()
                      << '\n';
        }
        if (error.scan_error.has_value())
        {
            for (const Generator::ClangDiagnostic& diagnostic :
                 error.scan_error->diagnostics)
            {
                PrintDiagnostic(diagnostic);
            }
        }
        if (error.render_error.has_value())
        {
            for (const Generator::ClangDeclarationValidationError& detail :
                 error.render_error->declaration_errors)
            {
                std::cerr << "  invalid declaration";
                if (!detail.source_file.empty())
                {
                    std::cerr << " at " << detail.source_file.string()
                              << ':' << detail.line << ':' << detail.column;
                }
                if (!detail.record_qualified_name.empty())
                {
                    std::cerr << " in " << detail.record_qualified_name;
                }
                if (!detail.field_name.empty())
                {
                    std::cerr << " field " << detail.field_name;
                }
                std::cerr << '\n';
            }
        }
        if (error.write_error.has_value())
        {
            if (error.write_error->filesystem_error)
            {
                std::cerr << "  filesystem: "
                          << error.write_error->filesystem_error.message()
                          << '\n';
            }
            if (!error.write_error->temporary_path.empty())
            {
                std::cerr << "  temporary path: "
                          << error.write_error->temporary_path.string()
                          << '\n';
            }
        }
    }

    int Run(int argument_count, char* arguments[])
    {
        const auto parse_result = ParseCommandLine(
            argument_count,
            arguments);
        const auto* command_line_error =
            std::get_if<CommandLineError>(&parse_result);
        if (command_line_error != nullptr)
        {
            std::cerr << "Error: " << command_line_error->message << "\n\n";
            PrintUsage(std::cerr);
            return 2;
        }

        const CommandLineOptions& options =
            std::get<CommandLineOptions>(parse_result);
        if (options.show_help)
        {
            PrintUsage(std::cout);
            return 0;
        }

        const Generator::ReflectionGenerationResult generation_result =
            Generator::RunReflectionGeneration(
                Generator::ReflectionGenerationRequest{
                    options.compilation_database_path,
                    options.manifest_paths});
        const auto* generation_error =
            std::get_if<Generator::ReflectionGenerationError>(
                &generation_result);
        if (generation_error != nullptr)
        {
            PrintGenerationError(*generation_error);
            return 1;
        }

        const Generator::ReflectionGenerationOutput& output =
            std::get<Generator::ReflectionGenerationOutput>(
                generation_result);
        for (const Generator::ClangDiagnostic& diagnostic :
             output.diagnostics)
        {
            PrintDiagnostic(diagnostic);
        }
        for (const Generator::GeneratedFileWriteRecord& file : output.files)
        {
            std::cout
                << (file.disposition ==
                            Generator::GeneratedFileWriteDisposition::Written
                        ? "written: "
                        : "unchanged: ")
                << file.path.string() << '\n';
        }
        std::cout << "Reflection generation completed: "
                  << output.module_count << " module(s), "
                  << output.header_count << " header(s), "
                  << output.reflected_type_count << " reflected type(s).\n";
        return 0;
    }
}

int main(int argument_count, char* arguments[])
{
    return Run(argument_count, arguments);
}
