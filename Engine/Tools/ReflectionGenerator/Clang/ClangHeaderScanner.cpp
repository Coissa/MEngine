#include "ClangHeaderScanner.h"

#include <clang/AST/Attr.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/FileSystemOptions.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Path.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace GE::Reflection::Generator
{
    namespace
    {
        struct SourcePosition
        {
            std::filesystem::path source_file;
            std::uint32_t line;
            std::uint32_t column;
        };

        std::filesystem::path Utf8Path(llvm::StringRef value)
        {
            return std::filesystem::u8path(value.begin(), value.end());
        }

        std::string Utf8String(const std::filesystem::path& value)
        {
            const std::u8string utf8 = value.u8string();
            return std::string{
                reinterpret_cast<const char*>(utf8.data()),
                utf8.size()};
        }

        bool HasExplicitDriverMode(
            const std::vector<std::string>& arguments)
        {
            for (const std::string& argument : arguments)
            {
                if (llvm::StringRef{argument}.starts_with("--driver-mode="))
                {
                    return true;
                }
            }

            return false;
        }

        bool UsesClDriver(const std::vector<std::string>& arguments)
        {
            for (const std::string& argument : arguments)
            {
                llvm::StringRef driver_mode{argument};
                if (driver_mode.consume_front("--driver-mode="))
                {
                    return driver_mode.equals_insensitive("cl");
                }
            }

            const llvm::StringRef compiler_name =
                llvm::sys::path::filename(
                    arguments.front(),
                    llvm::sys::path::Style::windows);
            return compiler_name.equals_insensitive("cl")
                || compiler_name.equals_insensitive("cl.exe")
                || compiler_name.equals_insensitive("clang-cl")
                || compiler_name.equals_insensitive("clang-cl.exe");
        }

        bool IsClOutputPathOption(llvm::StringRef argument)
        {
            constexpr llvm::StringLiteral prefixes[]{
                "/Fo", "-Fo", "/Fd", "-Fd", "/Fe", "-Fe",
                "/Fa", "-Fa", "/Fi", "-Fi"};

            for (const llvm::StringRef prefix : prefixes)
            {
                if (argument.starts_with(prefix))
                {
                    return true;
                }
            }

            return false;
        }

        std::vector<std::string> StripClOutputArguments(
            const std::vector<std::string>& arguments)
        {
            std::vector<std::string> adjusted_arguments;
            adjusted_arguments.reserve(arguments.size());

            for (std::size_t index = 0; index < arguments.size(); ++index)
            {
                const llvm::StringRef argument{arguments[index]};
                if (argument.equals_insensitive("/c")
                    || argument.equals_insensitive("-c"))
                {
                    continue;
                }

                if (IsClOutputPathOption(argument))
                {
                    if (argument.size() == 3
                        && index + 1 < arguments.size())
                    {
                        ++index;
                    }
                    continue;
                }

                adjusted_arguments.push_back(arguments[index]);
            }

            return adjusted_arguments;
        }

        std::vector<std::string> PrepareToolArguments(
            const HeaderParseInvocation& invocation)
        {
            const std::string header_path = Utf8String(
                invocation.job.header_path);

            std::vector<std::string> arguments = invocation.arguments;
            const bool uses_cl_driver = UsesClDriver(arguments);
            if (uses_cl_driver && !HasExplicitDriverMode(arguments))
            {
                arguments.insert(arguments.begin() + 1, "--driver-mode=cl");
            }

            if (uses_cl_driver)
            {
                arguments = StripClOutputArguments(arguments);
            }

            arguments =
                clang::tooling::getClangStripOutputAdjuster()(
                    arguments,
                    header_path);
            arguments =
                clang::tooling::getClangStripDependencyFileAdjuster()(
                    arguments,
                    header_path);
            arguments = clang::tooling::getClangSyntaxOnlyAdjuster()(
                arguments,
                header_path);
            arguments = clang::tooling::getInsertArgumentAdjuster(
                "-Wno-pragma-once-outside-header")(
                    arguments,
                    header_path);
            return arguments;
        }

        std::optional<SourcePosition> GetSourcePosition(
            const clang::SourceManager& source_manager,
            clang::SourceLocation location)
        {
            if (location.isInvalid())
            {
                return std::nullopt;
            }

            const clang::SourceLocation expansion_location =
                source_manager.getExpansionLoc(location);
            if (expansion_location.isInvalid())
            {
                return std::nullopt;
            }

            const clang::PresumedLoc presumed_location =
                source_manager.getPresumedLoc(expansion_location);
            if (presumed_location.isInvalid())
            {
                return std::nullopt;
            }

            return SourcePosition{
                Utf8Path(presumed_location.getFilename()),
                static_cast<std::uint32_t>(presumed_location.getLine()),
                static_cast<std::uint32_t>(presumed_location.getColumn())};
        }

        constexpr llvm::StringLiteral ReflectedTypeAnnotation =
            "mengine.reflect.type";
        constexpr llvm::StringLiteral ReflectedFieldAnnotation =
            "mengine.reflect.field";

        bool HasAnnotation(
            const clang::Decl& declaration,
            llvm::StringRef expected_annotation)
        {
            for (const clang::AnnotateAttr* annotation :
                 declaration.specific_attrs<clang::AnnotateAttr>())
            {
                if (annotation->getAnnotation() == expected_annotation)
                {
                    return true;
                }
            }

            return false;
        }

        ClangFieldAccess ConvertFieldAccess(clang::AccessSpecifier access)
        {
            switch (access)
            {
            case clang::AS_public:
                return ClangFieldAccess::Public;
            case clang::AS_protected:
                return ClangFieldAccess::Protected;
            case clang::AS_private:
                return ClangFieldAccess::Private;
            case clang::AS_none:
                return ClangFieldAccess::Unspecified;
            }

            return ClangFieldAccess::Unspecified;
        }

        std::vector<ClangFieldDeclaration> CollectReflectedFields(
            const clang::CXXRecordDecl& record,
            const clang::SourceManager& source_manager)
        {
            std::vector<ClangFieldDeclaration> fields;
            const clang::PrintingPolicy printing_policy =
                record.getASTContext().getPrintingPolicy();

            for (const clang::FieldDecl* field : record.fields())
            {
                if (field == nullptr
                    || !HasAnnotation(*field, ReflectedFieldAnnotation))
                {
                    continue;
                }

                const clang::SourceLocation expansion_location =
                    source_manager.getExpansionLoc(field->getLocation());
                if (expansion_location.isInvalid()
                    || !source_manager.isWrittenInMainFile(
                        expansion_location))
                {
                    continue;
                }

                const std::optional<SourcePosition> source_position =
                    GetSourcePosition(source_manager, expansion_location);
                if (!source_position.has_value())
                {
                    continue;
                }

                fields.push_back(ClangFieldDeclaration{
                    field->getNameAsString(),
                    field->getType().getAsString(printing_policy),
                    source_position->source_file,
                    source_position->line,
                    source_position->column,
                    ConvertFieldAccess(field->getAccess()),
                    field->getType().isConstQualified(),
                    field->isBitField()});
            }

            return fields;
        }

        class HeaderRecordVisitor final
            : public clang::RecursiveASTVisitor<HeaderRecordVisitor>
        {
        public:
            HeaderRecordVisitor(
                clang::SourceManager& source_manager,
                std::vector<ClangRecordDeclaration>& records)
                : source_manager_(source_manager),
                  records_(records)
            {
            }

            bool VisitCXXRecordDecl(clang::CXXRecordDecl* declaration)
            {
                if (declaration == nullptr
                    || !declaration->isThisDeclarationADefinition()
                    || declaration->isImplicit()
                    || (!declaration->isClass() && !declaration->isStruct())
                    || !HasAnnotation(
                        *declaration,
                        ReflectedTypeAnnotation))
                {
                    return true;
                }

                const clang::SourceLocation expansion_location =
                    source_manager_.getExpansionLoc(declaration->getLocation());
                if (expansion_location.isInvalid()
                    || !source_manager_.isWrittenInMainFile(expansion_location))
                {
                    return true;
                }

                const std::optional<SourcePosition> source_position =
                    GetSourcePosition(source_manager_, expansion_location);
                if (!source_position.has_value())
                {
                    return true;
                }

                std::vector<ClangFieldDeclaration> fields =
                    CollectReflectedFields(*declaration, source_manager_);

                records_.push_back(ClangRecordDeclaration{
                    declaration->isClass()
                        ? ClangRecordDeclarationKind::Class
                        : ClangRecordDeclarationKind::Struct,
                    declaration->getQualifiedNameAsString(),
                    source_position->source_file,
                    source_position->line,
                    source_position->column,
                    std::move(fields)});

                return true;
            }

        private:
            clang::SourceManager& source_manager_;
            std::vector<ClangRecordDeclaration>& records_;
        };

        class HeaderASTConsumer final : public clang::ASTConsumer
        {
        public:
            HeaderASTConsumer(
                clang::SourceManager& source_manager,
                std::vector<ClangRecordDeclaration>& records)
                : visitor_(source_manager, records)
            {
            }

            void HandleTranslationUnit(clang::ASTContext& context) override
            {
                visitor_.TraverseDecl(context.getTranslationUnitDecl());
            }

        private:
            HeaderRecordVisitor visitor_;
        };

        class HeaderFrontendAction final : public clang::ASTFrontendAction
        {
        public:
            HeaderFrontendAction(
                std::vector<ClangRecordDeclaration>& records,
                bool& consumer_created)
                : records_(records),
                  consumer_created_(consumer_created)
            {
            }

            std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
                clang::CompilerInstance& compiler,
                llvm::StringRef) override
            {
                consumer_created_ = true;
                return std::make_unique<HeaderASTConsumer>(
                    compiler.getSourceManager(),
                    records_);
            }

        private:
            std::vector<ClangRecordDeclaration>& records_;
            bool& consumer_created_;
        };

        ClangDiagnosticSeverity ConvertDiagnosticSeverity(
            clang::DiagnosticsEngine::Level level)
        {
            switch (level)
            {
            case clang::DiagnosticsEngine::Ignored:
            case clang::DiagnosticsEngine::Note:
            case clang::DiagnosticsEngine::Remark:
                return ClangDiagnosticSeverity::Note;
            case clang::DiagnosticsEngine::Warning:
                return ClangDiagnosticSeverity::Warning;
            case clang::DiagnosticsEngine::Error:
                return ClangDiagnosticSeverity::Error;
            case clang::DiagnosticsEngine::Fatal:
                return ClangDiagnosticSeverity::Fatal;
            }

            return ClangDiagnosticSeverity::Fatal;
        }

        class CollectingDiagnosticConsumer final
            : public clang::DiagnosticConsumer
        {
        public:
            explicit CollectingDiagnosticConsumer(
                std::vector<ClangDiagnostic>& diagnostics)
                : diagnostics_(diagnostics)
            {
            }

            void HandleDiagnostic(
                clang::DiagnosticsEngine::Level level,
                const clang::Diagnostic& diagnostic) override
            {
                clang::DiagnosticConsumer::HandleDiagnostic(level, diagnostic);

                std::filesystem::path source_file;
                std::uint32_t line = 0;
                std::uint32_t column = 0;

                if (diagnostic.hasSourceManager())
                {
                    const std::optional<SourcePosition> source_position =
                        GetSourcePosition(
                            diagnostic.getSourceManager(),
                            diagnostic.getLocation());
                    if (source_position.has_value())
                    {
                        source_file = source_position->source_file;
                        line = source_position->line;
                        column = source_position->column;
                    }
                }

                llvm::SmallString<256> formatted_message;
                diagnostic.FormatDiagnostic(formatted_message);

                diagnostics_.push_back(ClangDiagnostic{
                    ConvertDiagnosticSeverity(level),
                    std::move(source_file),
                    line,
                    column,
                    std::string{
                        formatted_message.data(),
                        formatted_message.size()}});
            }

        private:
            std::vector<ClangDiagnostic>& diagnostics_;
        };
    }

    ClangHeaderScanResult ScanHeaderWithClang(
        const HeaderParseInvocation& invocation)
    {
        if (invocation.arguments.empty())
        {
            return ClangHeaderScanError{
                ClangHeaderScanErrorCode::EmptyArguments,
                invocation.job.module_name,
                invocation.job.header_path,
                {}};
        }

        if (!invocation.job.header_path.is_absolute())
        {
            return ClangHeaderScanError{
                ClangHeaderScanErrorCode::HeaderPathNotAbsolute,
                invocation.job.module_name,
                invocation.job.header_path,
                {}};
        }

        if (!invocation.working_directory.is_absolute())
        {
            return ClangHeaderScanError{
                ClangHeaderScanErrorCode::WorkingDirectoryNotAbsolute,
                invocation.job.module_name,
                invocation.job.header_path,
                {}};
        }

        std::vector<ClangRecordDeclaration> records;
        std::vector<ClangDiagnostic> diagnostics;
        bool consumer_created = false;

        clang::FileSystemOptions file_system_options;
        file_system_options.WorkingDir = Utf8String(
            invocation.working_directory);
        llvm::IntrusiveRefCntPtr<clang::FileManager> file_manager =
            llvm::makeIntrusiveRefCnt<clang::FileManager>(
                file_system_options);

        auto frontend_action = std::make_unique<HeaderFrontendAction>(
            records,
            consumer_created);
        CollectingDiagnosticConsumer diagnostic_consumer(diagnostics);

        std::vector<std::string> tool_arguments =
            PrepareToolArguments(invocation);

        clang::tooling::ToolInvocation tool_invocation(
            std::move(tool_arguments),
            std::move(frontend_action),
            file_manager.get());
        tool_invocation.setDiagnosticConsumer(&diagnostic_consumer);

        if (!tool_invocation.run())
        {
            return ClangHeaderScanError{
                consumer_created
                    ? ClangHeaderScanErrorCode::ToolExecutionFailed
                    : ClangHeaderScanErrorCode::TranslationUnitCreationFailed,
                invocation.job.module_name,
                invocation.job.header_path,
                std::move(diagnostics)};
        }

        return ClangHeaderScanOutput{
            invocation.job.module_name,
            invocation.job.header_path,
            std::move(records),
            std::move(diagnostics)};
    }

}
