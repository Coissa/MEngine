#pragma once

#include "CompileCommand.h"

#include <filesystem>
#include <optional>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <cstddef>
#include <limits>

namespace GE::Reflection::Generator
{
    struct CompilationDatabase
    {
        std::unordered_map<std::filesystem::path, CompileCommand> commands_by_source;
    };

    enum class CompilationDatabaseLoadErrorCode
    {
        FileCannotBeOpened,
        FileReadError,
        DocumentParseError,
        InvalidEntry,
        DuplicateSourceFile
    };

    struct CompilationDatabaseLoadError
    {
        CompilationDatabaseLoadErrorCode code;
        std::size_t entry_index;
        std::filesystem::path source_file;
        std::string message;
    };

    using CompilationDatabaseLoadResult = std::variant<CompilationDatabase, CompilationDatabaseLoadError>;
    [[nodiscard]]
    CompilationDatabaseLoadResult LoadCompilationDatabase(
        const std::filesystem::path& database_path);

    enum class CompileCommandLookupError
    {
        SourcePathNotAbsolute,
        CommandNotFound
    };



    using CompileCommandLookupResult =
    std::variant<
        std::reference_wrapper<const CompileCommand>,
        CompileCommandLookupError>;

    [[nodiscard]]
    CompileCommandLookupResult FindCompileCommand(
        const CompilationDatabase& database,
        const std::filesystem::path& source_file);
}