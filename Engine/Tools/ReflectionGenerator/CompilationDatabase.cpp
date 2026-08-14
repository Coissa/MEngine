#include "CompilationDatabase.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
#include <string_view>
#include <utility>

namespace GE::Reflection::Generator
{
    namespace
    {
        constexpr std::size_t NoEntryIndex =
            std::numeric_limits<std::size_t>::max();

        CompilationDatabaseLoadResult MakeError(
            CompilationDatabaseLoadErrorCode code,
            std::size_t entry_index,
            std::filesystem::path source_file,
            std::string message)
        {
            return CompilationDatabaseLoadError{
                code,
                entry_index,
                std::move(source_file),
                std::move(message)
            };
        }

        std::filesystem::path Utf8Path(const std::string& value)
        {
            return std::filesystem::u8path(value);
        }

        std::vector<std::string> SplitCommandLine(std::string_view command)
        {
            std::vector<std::string> arguments;
            std::string argument;
            bool in_quotes = false;
            bool argument_started = false;

            for (std::size_t index = 0; index < command.size();)
            {
                if (!in_quotes &&
                    (command[index] == ' ' || command[index] == '\t'))
                {
                    if (argument_started)
                    {
                        arguments.push_back(std::move(argument));
                        argument.clear();
                        argument_started = false;
                    }

                    ++index;
                    continue;
                }

                if (command[index] == '\\')
                {
                    const std::size_t slash_begin = index;
                    while (index < command.size() && command[index] == '\\')
                    {
                        ++index;
                    }

                    const std::size_t slash_count = index - slash_begin;
                    if (index < command.size() && command[index] == '"')
                    {
                        argument.append(slash_count / 2, '\\');
                        if (slash_count % 2 == 0)
                        {
                            in_quotes = !in_quotes;
                        }
                        else
                        {
                            argument.push_back('"');
                        }
                        ++index;
                    }
                    else
                    {
                        argument.append(slash_count, '\\');
                    }

                    argument_started = true;
                    continue;
                }

                if (command[index] == '"')
                {
                    in_quotes = !in_quotes;
                    argument_started = true;
                    ++index;
                    continue;
                }

                argument.push_back(command[index]);
                argument_started = true;
                ++index;
            }

            if (in_quotes)
            {
                return {};
            }

            if (argument_started)
            {
                arguments.push_back(std::move(argument));
            }

            return arguments;
        }

        bool ReadArguments(
            const nlohmann::json& entry,
            std::vector<std::string>& arguments)
        {
            const auto arguments_field = entry.find("arguments");
            if (arguments_field != entry.end())
            {
                if (!arguments_field->is_array())
                {
                    return false;
                }

                arguments.reserve(arguments_field->size());
                for (const nlohmann::json& argument : *arguments_field)
                {
                    if (!argument.is_string())
                    {
                        return false;
                    }

                    arguments.push_back(argument.get<std::string>());
                }

                return !arguments.empty();
            }

            const auto command_field = entry.find("command");
            if (command_field == entry.end() || !command_field->is_string())
            {
                return false;
            }

            arguments = SplitCommandLine(command_field->get<std::string>());
            return !arguments.empty();
        }
    }

    CompilationDatabaseLoadResult LoadCompilationDatabase(
        const std::filesystem::path& database_path)
    {
        std::ifstream input(database_path, std::ios::binary);
        if (!input.is_open())
        {
            return MakeError(
                CompilationDatabaseLoadErrorCode::FileCannotBeOpened,
                NoEntryIndex,
                {},
                "Cannot open compilation database file");
        }

        const std::string contents{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}
        };

        if (input.bad())
        {
            return MakeError(
                CompilationDatabaseLoadErrorCode::FileReadError,
                NoEntryIndex,
                {},
                "Failed while reading compilation database file");
        }

        nlohmann::json document;
        try
        {
            document = nlohmann::json::parse(contents);
        }
        catch (const nlohmann::json::parse_error& error)
        {
            return MakeError(
                CompilationDatabaseLoadErrorCode::DocumentParseError,
                NoEntryIndex,
                {},
                error.what());
        }

        if (!document.is_array())
        {
            return MakeError(
                CompilationDatabaseLoadErrorCode::InvalidEntry,
                NoEntryIndex,
                {},
                "Compilation database root must be a JSON array");
        }

        CompilationDatabase database;
        database.commands_by_source.reserve(document.size());
        const std::filesystem::path database_directory =
            database_path.parent_path();

        for (std::size_t entry_index = 0;
             entry_index < document.size();
             ++entry_index)
        {
            const nlohmann::json& entry = document[entry_index];
            if (!entry.is_object())
            {
                return MakeError(
                    CompilationDatabaseLoadErrorCode::InvalidEntry,
                    entry_index,
                    {},
                    "Compilation database entry must be an object");
            }

            const auto directory_field = entry.find("directory");
            const auto file_field = entry.find("file");
            if (directory_field == entry.end() ||
                !directory_field->is_string() ||
                file_field == entry.end() ||
                !file_field->is_string())
            {
                return MakeError(
                    CompilationDatabaseLoadErrorCode::InvalidEntry,
                    entry_index,
                    {},
                    "Entry fields 'directory' and 'file' must be strings");
            }

            std::filesystem::path working_directory =
                Utf8Path(directory_field->get<std::string>());
            if (working_directory.is_relative())
            {
                working_directory = database_directory / working_directory;
            }
            working_directory = working_directory.lexically_normal();

            std::filesystem::path source_file =
                Utf8Path(file_field->get<std::string>());
            if (source_file.is_relative())
            {
                source_file = working_directory / source_file;
            }
            source_file = source_file.lexically_normal();

            std::vector<std::string> arguments;
            if (!ReadArguments(entry, arguments))
            {
                return MakeError(
                    CompilationDatabaseLoadErrorCode::InvalidEntry,
                    entry_index,
                    source_file,
                    "Entry must contain a non-empty valid 'arguments' array or 'command' string");
            }

            CompileCommand command{
                std::move(working_directory),
                source_file,
                std::move(arguments)
            };

            if (!database.commands_by_source
                    .emplace(source_file, std::move(command))
                    .second)
            {
                return MakeError(
                    CompilationDatabaseLoadErrorCode::DuplicateSourceFile,
                    entry_index,
                    source_file,
                    "Duplicate source file in compilation database");
            }
        }

        return database;
    }

    CompileCommandLookupResult FindCompileCommand(
        const CompilationDatabase& database,
        const std::filesystem::path& source_file)
    {
        if (!source_file.is_absolute())
        {
            return CompileCommandLookupError::SourcePathNotAbsolute;
        }

        const auto command = database.commands_by_source.find(
            source_file.lexically_normal());
        if (command == database.commands_by_source.end())
        {
            return CompileCommandLookupError::CommandNotFound;
        }

        return std::cref(command->second);
    }
}
