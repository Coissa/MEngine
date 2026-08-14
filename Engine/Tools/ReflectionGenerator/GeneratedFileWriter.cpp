#include "GeneratedFileWriter.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace GE::Reflection::Generator
{
    namespace
    {
        constexpr std::size_t MaximumTemporaryPathAttempts = 64;
        std::atomic<std::uint64_t> temporary_file_sequence{0};

        struct StagedFile
        {
            std::size_t input_index;
            std::filesystem::path destination_path;
            std::filesystem::path temporary_path;
        };

        struct TemporaryFileWriteError
        {
            GeneratedFileWriteErrorCode code;
            std::filesystem::path temporary_path;
            std::error_code filesystem_error;
        };

        using TemporaryFileWriteResult =
            std::variant<std::filesystem::path, TemporaryFileWriteError>;

        std::filesystem::path NormalizePath(
            const std::filesystem::path& path)
        {
            return path.lexically_normal();
        }

#if defined(_WIN32)
        std::wstring BuildPathIdentity(const std::filesystem::path& path)
        {
            std::wstring identity = NormalizePath(path).native();
            std::transform(
                identity.begin(),
                identity.end(),
                identity.begin(),
                [](wchar_t character)
                {
                    return static_cast<wchar_t>(
                        std::towlower(static_cast<wint_t>(character)));
                });
            return identity;
        }
#else
        std::string BuildPathIdentity(const std::filesystem::path& path)
        {
            return NormalizePath(path).native();
        }
#endif

        std::filesystem::path BuildTemporaryPath(
            const std::filesystem::path& destination_path)
        {
            const std::uint64_t timestamp =
                static_cast<std::uint64_t>(
                    std::chrono::steady_clock::now()
                        .time_since_epoch()
                        .count());
            const std::uint64_t sequence =
                temporary_file_sequence.fetch_add(
                    1,
                    std::memory_order_relaxed);

            std::filesystem::path temporary_path = destination_path;
            temporary_path += ".tmp.";
            temporary_path += std::to_string(timestamp);
            temporary_path += ".";
            temporary_path += std::to_string(sequence);
            return temporary_path;
        }

        std::vector<GeneratedFileWriteRecord> CollectCompletedRecords(
            const std::vector<std::optional<GeneratedFileWriteRecord>>& records)
        {
            std::vector<GeneratedFileWriteRecord> completed;
            completed.reserve(records.size());
            for (const std::optional<GeneratedFileWriteRecord>& record : records)
            {
                if (record.has_value())
                {
                    completed.push_back(*record);
                }
            }
            return completed;
        }

        GeneratedFilesWriteResult MakeError(
            GeneratedFileWriteErrorCode code,
            std::filesystem::path path,
            std::filesystem::path temporary_path,
            std::error_code filesystem_error,
            const std::vector<std::optional<GeneratedFileWriteRecord>>& records)
        {
            return GeneratedFileWriteError{
                code,
                std::move(path),
                std::move(temporary_path),
                filesystem_error,
                CollectCompletedRecords(records)};
        }

        void RemoveTemporaryFile(const std::filesystem::path& path) noexcept
        {
            std::error_code ignored_error;
            std::filesystem::remove(path, ignored_error);
        }

        void RemoveStagedFiles(
            const std::vector<StagedFile>& staged_files,
            std::size_t first_index = 0) noexcept
        {
            for (std::size_t index = first_index;
                 index < staged_files.size();
                 ++index)
            {
                RemoveTemporaryFile(staged_files[index].temporary_path);
            }
        }

        std::optional<std::string> ReadExistingContents(
            const std::filesystem::path& path,
            std::error_code& filesystem_error)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
            {
                filesystem_error = std::make_error_code(
                    std::errc::io_error);
                return std::nullopt;
            }

            std::string contents{
                std::istreambuf_iterator<char>{input},
                std::istreambuf_iterator<char>{}};
            if (input.bad())
            {
                filesystem_error = std::make_error_code(
                    std::errc::io_error);
                return std::nullopt;
            }

            filesystem_error.clear();
            return contents;
        }

#if defined(_WIN32)
        TemporaryFileWriteResult WriteTemporaryFile(
            const std::filesystem::path& destination_path,
            const std::string& contents)
        {
            for (std::size_t attempt = 0;
                 attempt < MaximumTemporaryPathAttempts;
                 ++attempt)
            {
                const std::filesystem::path temporary_path =
                    BuildTemporaryPath(destination_path);
                HANDLE handle = CreateFileW(
                    temporary_path.c_str(),
                    GENERIC_WRITE,
                    0,
                    nullptr,
                    CREATE_NEW,
                    FILE_ATTRIBUTE_NORMAL,
                    nullptr);
                if (handle == INVALID_HANDLE_VALUE)
                {
                    const DWORD native_error = GetLastError();
                    if (native_error == ERROR_FILE_EXISTS
                        || native_error == ERROR_ALREADY_EXISTS)
                    {
                        continue;
                    }

                    return TemporaryFileWriteError{
                        GeneratedFileWriteErrorCode::
                            CreateTemporaryFileFailed,
                        temporary_path,
                        std::error_code{
                            static_cast<int>(native_error),
                            std::system_category()}};
                }

                std::size_t offset = 0;
                while (offset < contents.size())
                {
                    const std::size_t remaining = contents.size() - offset;
                    const DWORD requested = static_cast<DWORD>(std::min(
                        remaining,
                        static_cast<std::size_t>(
                            std::numeric_limits<DWORD>::max())));
                    DWORD written = 0;
                    if (!WriteFile(
                            handle,
                            contents.data() + offset,
                            requested,
                            &written,
                            nullptr)
                        || written == 0)
                    {
                        const DWORD native_error = GetLastError();
                        CloseHandle(handle);
                        RemoveTemporaryFile(temporary_path);
                        return TemporaryFileWriteError{
                            GeneratedFileWriteErrorCode::
                                WriteTemporaryFileFailed,
                            temporary_path,
                            std::error_code{
                                static_cast<int>(native_error),
                                std::system_category()}};
                    }
                    offset += written;
                }

                if (!FlushFileBuffers(handle))
                {
                    const DWORD native_error = GetLastError();
                    CloseHandle(handle);
                    RemoveTemporaryFile(temporary_path);
                    return TemporaryFileWriteError{
                        GeneratedFileWriteErrorCode::
                            FlushTemporaryFileFailed,
                        temporary_path,
                        std::error_code{
                            static_cast<int>(native_error),
                            std::system_category()}};
                }

                if (!CloseHandle(handle))
                {
                    const DWORD native_error = GetLastError();
                    RemoveTemporaryFile(temporary_path);
                    return TemporaryFileWriteError{
                        GeneratedFileWriteErrorCode::
                            WriteTemporaryFileFailed,
                        temporary_path,
                        std::error_code{
                            static_cast<int>(native_error),
                            std::system_category()}};
                }

                return temporary_path;
            }

            return TemporaryFileWriteError{
                GeneratedFileWriteErrorCode::CreateTemporaryFileFailed,
                destination_path,
                std::make_error_code(std::errc::file_exists)};
        }

        std::error_code CommitTemporaryFile(
            const std::filesystem::path& temporary_path,
            const std::filesystem::path& destination_path)
        {
            if (MoveFileExW(
                    temporary_path.c_str(),
                    destination_path.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                return {};
            }

            return std::error_code{
                static_cast<int>(GetLastError()),
                std::system_category()};
        }
#else
        TemporaryFileWriteResult WriteTemporaryFile(
            const std::filesystem::path& destination_path,
            const std::string& contents)
        {
            for (std::size_t attempt = 0;
                 attempt < MaximumTemporaryPathAttempts;
                 ++attempt)
            {
                const std::filesystem::path temporary_path =
                    BuildTemporaryPath(destination_path);
                const int descriptor = ::open(
                    temporary_path.c_str(),
                    O_WRONLY | O_CREAT | O_EXCL,
                    0666);
                if (descriptor < 0)
                {
                    if (errno == EEXIST)
                    {
                        continue;
                    }

                    return TemporaryFileWriteError{
                        GeneratedFileWriteErrorCode::
                            CreateTemporaryFileFailed,
                        temporary_path,
                        std::error_code{errno, std::generic_category()}};
                }

                std::size_t offset = 0;
                while (offset < contents.size())
                {
                    const ssize_t written = ::write(
                        descriptor,
                        contents.data() + offset,
                        contents.size() - offset);
                    if (written <= 0)
                    {
                        const int native_error = errno;
                        ::close(descriptor);
                        RemoveTemporaryFile(temporary_path);
                        return TemporaryFileWriteError{
                            GeneratedFileWriteErrorCode::
                                WriteTemporaryFileFailed,
                            temporary_path,
                            std::error_code{
                                native_error,
                                std::generic_category()}};
                    }
                    offset += static_cast<std::size_t>(written);
                }

                if (::fsync(descriptor) != 0)
                {
                    const int native_error = errno;
                    ::close(descriptor);
                    RemoveTemporaryFile(temporary_path);
                    return TemporaryFileWriteError{
                        GeneratedFileWriteErrorCode::
                            FlushTemporaryFileFailed,
                        temporary_path,
                        std::error_code{
                            native_error,
                            std::generic_category()}};
                }

                if (::close(descriptor) != 0)
                {
                    const int native_error = errno;
                    RemoveTemporaryFile(temporary_path);
                    return TemporaryFileWriteError{
                        GeneratedFileWriteErrorCode::
                            WriteTemporaryFileFailed,
                        temporary_path,
                        std::error_code{
                            native_error,
                            std::generic_category()}};
                }

                return temporary_path;
            }

            return TemporaryFileWriteError{
                GeneratedFileWriteErrorCode::CreateTemporaryFileFailed,
                destination_path,
                std::make_error_code(std::errc::file_exists)};
        }

        std::error_code CommitTemporaryFile(
            const std::filesystem::path& temporary_path,
            const std::filesystem::path& destination_path)
        {
            std::error_code filesystem_error;
            std::filesystem::rename(
                temporary_path,
                destination_path,
                filesystem_error);
            return filesystem_error;
        }
#endif
    }

    GeneratedFilesWriteResult WriteGeneratedCodeFiles(
        std::span<const GeneratedCodeFile> files)
    {
        std::vector<std::filesystem::path> normalized_paths;
        normalized_paths.reserve(files.size());

        std::vector<decltype(BuildPathIdentity(std::filesystem::path{}))>
            path_identities;
        path_identities.reserve(files.size());

        std::vector<std::optional<GeneratedFileWriteRecord>> records(
            files.size());

        for (const GeneratedCodeFile& file : files)
        {
            if (file.path.empty())
            {
                return MakeError(
                    GeneratedFileWriteErrorCode::EmptyPath,
                    file.path,
                    {},
                    {},
                    records);
            }
            if (!file.path.is_absolute())
            {
                return MakeError(
                    GeneratedFileWriteErrorCode::PathNotAbsolute,
                    file.path,
                    {},
                    {},
                    records);
            }

            const std::filesystem::path normalized_path =
                NormalizePath(file.path);
            auto identity = BuildPathIdentity(normalized_path);
            if (std::find(
                    path_identities.begin(),
                    path_identities.end(),
                    identity)
                != path_identities.end())
            {
                return MakeError(
                    GeneratedFileWriteErrorCode::DuplicateOutputPath,
                    normalized_path,
                    {},
                    {},
                    records);
            }

            normalized_paths.push_back(normalized_path);
            path_identities.push_back(std::move(identity));
        }

        std::vector<StagedFile> staged_files;
        staged_files.reserve(files.size());

        for (std::size_t index = 0; index < files.size(); ++index)
        {
            const GeneratedCodeFile& file = files[index];
            const std::filesystem::path& destination_path =
                normalized_paths[index];
            const std::filesystem::path parent_path =
                destination_path.parent_path();

            std::error_code filesystem_error;
            std::filesystem::create_directories(
                parent_path,
                filesystem_error);
            if (filesystem_error)
            {
                RemoveStagedFiles(staged_files);
                return MakeError(
                    GeneratedFileWriteErrorCode::
                        CreateParentDirectoryFailed,
                    destination_path,
                    {},
                    filesystem_error,
                    records);
            }

            const std::filesystem::file_status destination_status =
                std::filesystem::symlink_status(
                    destination_path,
                    filesystem_error);
            const bool destination_not_found =
                destination_status.type()
                == std::filesystem::file_type::not_found;
            if (filesystem_error && !destination_not_found)
            {
                RemoveStagedFiles(staged_files);
                return MakeError(
                    GeneratedFileWriteErrorCode::InspectDestinationFailed,
                    destination_path,
                    {},
                    filesystem_error,
                    records);
            }

            const bool destination_exists =
                !destination_not_found
                && std::filesystem::exists(destination_status);
            filesystem_error.clear();

            if (destination_exists)
            {
                if (!std::filesystem::is_regular_file(destination_status))
                {
                    RemoveStagedFiles(staged_files);
                    return MakeError(
                        GeneratedFileWriteErrorCode::
                            DestinationNotRegularFile,
                        destination_path,
                        {},
                        {},
                        records);
                }

                const std::optional<std::string> existing_contents =
                    ReadExistingContents(
                        destination_path,
                        filesystem_error);
                if (!existing_contents.has_value())
                {
                    RemoveStagedFiles(staged_files);
                    return MakeError(
                        GeneratedFileWriteErrorCode::
                            ReadDestinationFailed,
                        destination_path,
                        {},
                        filesystem_error,
                        records);
                }

                if (*existing_contents == file.contents)
                {
                    records[index] = GeneratedFileWriteRecord{
                        destination_path,
                        GeneratedFileWriteDisposition::Unchanged};
                    continue;
                }
            }

            TemporaryFileWriteResult temporary_result =
                WriteTemporaryFile(destination_path, file.contents);
            const auto* temporary_error =
                std::get_if<TemporaryFileWriteError>(&temporary_result);
            if (temporary_error != nullptr)
            {
                RemoveStagedFiles(staged_files);
                return MakeError(
                    temporary_error->code,
                    destination_path,
                    temporary_error->temporary_path,
                    temporary_error->filesystem_error,
                    records);
            }

            staged_files.push_back(StagedFile{
                index,
                destination_path,
                std::get<std::filesystem::path>(
                    std::move(temporary_result))});
        }

        for (std::size_t index = 0;
             index < staged_files.size();
             ++index)
        {
            const StagedFile& staged = staged_files[index];
            const std::error_code filesystem_error = CommitTemporaryFile(
                staged.temporary_path,
                staged.destination_path);
            if (filesystem_error)
            {
                RemoveStagedFiles(staged_files, index);
                return MakeError(
                    GeneratedFileWriteErrorCode::
                        CommitTemporaryFileFailed,
                    staged.destination_path,
                    staged.temporary_path,
                    filesystem_error,
                    records);
            }

            records[staged.input_index] = GeneratedFileWriteRecord{
                staged.destination_path,
                GeneratedFileWriteDisposition::Written};
        }

        GeneratedFilesWriteOutput output;
        output.files.reserve(records.size());
        for (const std::optional<GeneratedFileWriteRecord>& record : records)
        {
            output.files.push_back(*record);
        }
        return output;
    }
}
