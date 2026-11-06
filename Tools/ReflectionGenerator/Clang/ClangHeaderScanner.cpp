#include "ClangHeaderScanner.h"

#include <clang/AST/Attr.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/QualTypeNames.h>
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
#include <iterator>
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
            return std::string{reinterpret_cast<const char*>(utf8.data()), utf8.size()};
        }

        bool HasExplicitDriverMode(const std::vector<std::string>& arguments)
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
                llvm::sys::path::filename(arguments.front(), llvm::sys::path::Style::windows);
            return compiler_name.equals_insensitive("cl") || compiler_name.equals_insensitive("cl.exe") ||
                   compiler_name.equals_insensitive("clang-cl") || compiler_name.equals_insensitive("clang-cl.exe");
        }

        bool IsClOutputPathOption(llvm::StringRef argument)
        {
            constexpr llvm::StringLiteral prefixes[]{"/Fo", "-Fo", "/Fd", "-Fd", "/Fe",
                                                     "-Fe", "/Fa", "-Fa", "/Fi", "-Fi"};

            for (const llvm::StringRef prefix : prefixes)
            {
                if (argument.starts_with(prefix))
                {
                    return true;
                }
            }

            return false;
        }

        std::vector<std::string> StripClOutputArguments(const std::vector<std::string>& arguments)
        {
            std::vector<std::string> adjusted_arguments;
            adjusted_arguments.reserve(arguments.size());

            for (std::size_t index = 0; index < arguments.size(); ++index)
            {
                const llvm::StringRef argument{arguments[index]};
                if (argument.equals_insensitive("/c") || argument.equals_insensitive("-c"))
                {
                    continue;
                }

                if (IsClOutputPathOption(argument))
                {
                    if (argument.size() == 3 && index + 1 < arguments.size())
                    {
                        ++index;
                    }
                    continue;
                }

                adjusted_arguments.push_back(arguments[index]);
            }

            return adjusted_arguments;
        }

        std::vector<std::string> PrepareToolArguments(const HeaderParseInvocation& invocation)
        {
            const std::string header_path = Utf8String(invocation.job.header_path);

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

            arguments = clang::tooling::getClangStripOutputAdjuster()(arguments, header_path);
            arguments = clang::tooling::getClangStripDependencyFileAdjuster()(arguments, header_path);
            arguments = clang::tooling::getClangSyntaxOnlyAdjuster()(arguments, header_path);
            arguments =
                clang::tooling::getInsertArgumentAdjuster("-Wno-pragma-once-outside-header")(arguments, header_path);
            return arguments;
        }

        std::optional<SourcePosition> GetSourcePosition(const clang::SourceManager& source_manager,
                                                        clang::SourceLocation location)
        {
            if (location.isInvalid())
            {
                return std::nullopt;
            }

            const clang::SourceLocation expansion_location = source_manager.getExpansionLoc(location);
            if (expansion_location.isInvalid())
            {
                return std::nullopt;
            }

            const clang::PresumedLoc presumed_location = source_manager.getPresumedLoc(expansion_location);
            if (presumed_location.isInvalid())
            {
                return std::nullopt;
            }

            return SourcePosition{Utf8Path(presumed_location.getFilename()),
                                  static_cast<std::uint32_t>(presumed_location.getLine()),
                                  static_cast<std::uint32_t>(presumed_location.getColumn())};
        }

        constexpr llvm::StringLiteral ReflectedTypeAnnotation = "mengine.reflect.type";
        constexpr llvm::StringLiteral ReflectedFieldAnnotation = "mengine.reflect.field";
        constexpr llvm::StringLiteral ReflectedFunctionAnnotation = "mengine.reflect.function";
        constexpr llvm::StringLiteral ReflectedEnumAnnotation = "mengine.reflect.enum";
        constexpr llvm::StringLiteral StableNameAnnotationPrefix = "mengine.reflect.stable_name:";
        constexpr llvm::StringLiteral SerializableAnnotation = "mengine.reflect.serializable";
        constexpr llvm::StringLiteral SerializedNameAnnotationPrefix = "mengine.reflect.serialized_name:";
        constexpr llvm::StringLiteral RequiredAnnotation = "mengine.reflect.required";
        constexpr llvm::StringLiteral TransientAnnotation = "mengine.reflect.transient";

        bool HasAnnotation(const clang::Decl& declaration, llvm::StringRef expected_annotation)
        {
            for (const clang::AnnotateAttr* annotation : declaration.specific_attrs<clang::AnnotateAttr>())
            {
                if (annotation->getAnnotation() == expected_annotation)
                {
                    return true;
                }
            }

            return false;
        }

        std::optional<std::string> GetAnnotationValue(const clang::Decl& declaration, llvm::StringRef prefix)
        {
            for (const clang::AnnotateAttr* annotation : declaration.specific_attrs<clang::AnnotateAttr>())
            {
                llvm::StringRef value = annotation->getAnnotation();
                if (value.consume_front(prefix))
                    return value.str();
            }
            return std::nullopt;
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

        std::vector<ClangFieldDeclaration> CollectReflectedFields(const clang::CXXRecordDecl& record,
                                                                  const clang::SourceManager& source_manager)
        {
            std::vector<ClangFieldDeclaration> fields;
            const clang::PrintingPolicy printing_policy = record.getASTContext().getPrintingPolicy();

            for (const clang::FieldDecl* field : record.fields())
            {
                if (field == nullptr || !HasAnnotation(*field, ReflectedFieldAnnotation))
                {
                    continue;
                }

                const clang::SourceLocation expansion_location = source_manager.getExpansionLoc(field->getLocation());
                if (expansion_location.isInvalid() || !source_manager.isWrittenInMainFile(expansion_location))
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
                    clang::TypeName::getFullyQualifiedName(field->getType(), record.getASTContext(), printing_policy,
                                                           true),
                    source_position->source_file, source_position->line, source_position->column,
                    ConvertFieldAccess(field->getAccess()), field->getType().isConstQualified(), field->isBitField(),
                    GetAnnotationValue(*field, SerializedNameAnnotationPrefix).value_or(std::string{}),
                    HasAnnotation(*field, RequiredAnnotation), HasAnnotation(*field, TransientAnnotation),
                    field->getType()->isPointerType(), field->getType()->isReferenceType(),
                    field->getType()->isArrayType(), HasAnnotation(*field, "mengine.reflect.copy"),
                    GetAnnotationValue(*field, "mengine.reflect.editor:").value_or(std::string{})});
            }

            return fields;
        }

        std::vector<ClangFunctionDeclaration> CollectReflectedFunctions(const clang::CXXRecordDecl& record,
                                                                        const clang::SourceManager& source_manager)
        {
            std::vector<ClangFunctionDeclaration> functions;
            const clang::PrintingPolicy printing_policy = record.getASTContext().getPrintingPolicy();

            for (const clang::CXXMethodDecl* method : record.methods())
            {
                if (method == nullptr || !HasAnnotation(*method, ReflectedFunctionAnnotation))
                {
                    continue;
                }

                const clang::SourceLocation expansion_location = source_manager.getExpansionLoc(method->getLocation());
                if (expansion_location.isInvalid() || !source_manager.isWrittenInMainFile(expansion_location))
                {
                    continue;
                }

                const std::optional<SourcePosition> source_position =
                    GetSourcePosition(source_manager, expansion_location);
                if (!source_position.has_value())
                {
                    continue;
                }

                std::vector<ClangParameterDeclaration> parameters;
                parameters.reserve(method->getNumParams());
                for (const clang::ParmVarDecl* parameter : method->parameters())
                {
                    const clang::SourceLocation parameter_location =
                        source_manager.getExpansionLoc(parameter->getLocation());
                    const std::optional<SourcePosition> parameter_position =
                        GetSourcePosition(source_manager, parameter_location);
                    const clang::QualType parameter_type = parameter->getType();
                    const clang::QualType referenced_type = parameter_type.getNonReferenceType();

                    parameters.push_back(ClangParameterDeclaration{
                        parameter->getNameAsString(),
                        clang::TypeName::getFullyQualifiedName(parameter_type, record.getASTContext(), printing_policy,
                                                               true),
                        parameter_position.has_value() ? parameter_position->source_file : source_position->source_file,
                        parameter_position.has_value() ? parameter_position->line : source_position->line,
                        parameter_position.has_value() ? parameter_position->column : source_position->column,
                        parameter_type->isLValueReferenceType(), parameter_type->isRValueReferenceType(),
                        referenced_type.isConstQualified()});
                }

                const clang::QualType return_type = method->getReturnType();
                functions.push_back(ClangFunctionDeclaration{
                    method->getNameAsString(),
                    clang::TypeName::getFullyQualifiedName(return_type, record.getASTContext(), printing_policy, true),
                    source_position->source_file, source_position->line, source_position->column,
                    ConvertFieldAccess(method->getAccess()), std::move(parameters), method->isConst(),
                    method->isStatic(), method->isVariadic(), method->isDeleted(), method->isVolatile(),
                    method->getRefQualifier() != clang::RQ_None,
                    method->getTemplatedKind() != clang::FunctionDecl::TK_NonTemplate,
                    llvm::isa<clang::CXXConstructorDecl>(method) || llvm::isa<clang::CXXDestructorDecl>(method) ||
                        llvm::isa<clang::CXXConversionDecl>(method),
                    method->isOverloadedOperator(), return_type->isVoidType(), return_type->isReferenceType()});
            }

            return functions;
        }

        class HeaderRecordVisitor final : public clang::RecursiveASTVisitor<HeaderRecordVisitor>
        {
        public:
            HeaderRecordVisitor(clang::SourceManager& source_manager, std::vector<ClangRecordDeclaration>& records,
                                std::vector<ClangEnumDeclaration>& enums)
                : source_manager_(source_manager), records_(records), enums_(enums)
            {
            }

            bool VisitCXXRecordDecl(clang::CXXRecordDecl* declaration)
            {
                if (declaration == nullptr || !declaration->isThisDeclarationADefinition() ||
                    declaration->isImplicit() || (!declaration->isClass() && !declaration->isStruct()) ||
                    !HasAnnotation(*declaration, ReflectedTypeAnnotation))
                {
                    return true;
                }

                const clang::SourceLocation expansion_location =
                    source_manager_.getExpansionLoc(declaration->getLocation());
                if (expansion_location.isInvalid() || !source_manager_.isWrittenInMainFile(expansion_location))
                {
                    return true;
                }

                const std::optional<SourcePosition> source_position =
                    GetSourcePosition(source_manager_, expansion_location);
                if (!source_position.has_value())
                {
                    return true;
                }

                std::vector<ClangFieldDeclaration> fields = CollectReflectedFields(*declaration, source_manager_);
                std::vector<ClangFunctionDeclaration> functions =
                    CollectReflectedFunctions(*declaration, source_manager_);

                auto stable_name = GetAnnotationValue(*declaration, StableNameAnnotationPrefix);
                std::uint32_t version = 1;
                for (const auto* member : declaration->decls())
                {
                    if (!stable_name)
                        stable_name = GetAnnotationValue(*member, StableNameAnnotationPrefix);
                    if (auto text = GetAnnotationValue(*member, "mengine.reflect.version:"))
                        version = static_cast<std::uint32_t>(std::stoul(*text));
                }
                const bool is_component = HasAnnotation(*declaration, "mengine.reflect.component");
                bool supported_base = declaration->getNumBases() == 1;
                if (supported_base)
                {
                    const auto* base = declaration->bases_begin()->getType()->getAsCXXRecordDecl();
                    supported_base = base && base->getQualifiedNameAsString() == "GE::Runtime::Component" &&
                                     !declaration->bases_begin()->isVirtual();
                }
                records_.push_back(ClangRecordDeclaration{
                    declaration->isClass() ? ClangRecordDeclarationKind::Class : ClangRecordDeclarationKind::Struct,
                    declaration->getQualifiedNameAsString(), source_position->source_file, source_position->line,
                    source_position->column, std::move(fields), std::move(functions),
                    stable_name.value_or(declaration->getQualifiedNameAsString()),
                    HasAnnotation(*declaration, SerializableAnnotation), stable_name.has_value(), is_component,
                    HasAnnotation(*declaration, "mengine.reflect.essential"), version, supported_base});

                return true;
            }

            bool VisitEnumDecl(clang::EnumDecl* declaration)
            {
                if (declaration == nullptr || !declaration->isThisDeclarationADefinition() ||
                    declaration->isImplicit() || !HasAnnotation(*declaration, ReflectedEnumAnnotation))
                    return true;

                const clang::SourceLocation expansion_location =
                    source_manager_.getExpansionLoc(declaration->getLocation());
                if (expansion_location.isInvalid() || !source_manager_.isWrittenInMainFile(expansion_location))
                    return true;
                const std::optional<SourcePosition> source_position =
                    GetSourcePosition(source_manager_, expansion_location);
                if (!source_position.has_value())
                    return true;

                const clang::PrintingPolicy printing_policy = declaration->getASTContext().getPrintingPolicy();
                std::vector<ClangEnumValueDeclaration> values;
                values.reserve(std::distance(declaration->enumerator_begin(), declaration->enumerator_end()));
                for (const clang::EnumConstantDecl* enumerator : declaration->enumerators())
                {
                    values.push_back({enumerator->getNameAsString(), enumerator->getInitVal().getZExtValue()});
                }
                enums_.push_back(ClangEnumDeclaration{
                    declaration->getQualifiedNameAsString(), declaration->getIntegerType().getAsString(printing_policy),
                    source_position->source_file, source_position->line, source_position->column, std::move(values),
                    GetAnnotationValue(*declaration, StableNameAnnotationPrefix)
                        .value_or(declaration->getQualifiedNameAsString())});
                return true;
            }

        private:
            clang::SourceManager& source_manager_;
            std::vector<ClangRecordDeclaration>& records_;
            std::vector<ClangEnumDeclaration>& enums_;
        };

        class HeaderASTConsumer final : public clang::ASTConsumer
        {
        public:
            HeaderASTConsumer(clang::SourceManager& source_manager, std::vector<ClangRecordDeclaration>& records,
                              std::vector<ClangEnumDeclaration>& enums)
                : visitor_(source_manager, records, enums)
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
            HeaderFrontendAction(std::vector<ClangRecordDeclaration>& records, std::vector<ClangEnumDeclaration>& enums,
                                 bool& consumer_created)
                : records_(records), enums_(enums), consumer_created_(consumer_created)
            {
            }

            std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler,
                                                                  llvm::StringRef) override
            {
                consumer_created_ = true;
                return std::make_unique<HeaderASTConsumer>(compiler.getSourceManager(), records_, enums_);
            }

        private:
            std::vector<ClangRecordDeclaration>& records_;
            std::vector<ClangEnumDeclaration>& enums_;
            bool& consumer_created_;
        };

        ClangDiagnosticSeverity ConvertDiagnosticSeverity(clang::DiagnosticsEngine::Level level)
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

        class CollectingDiagnosticConsumer final : public clang::DiagnosticConsumer
        {
        public:
            explicit CollectingDiagnosticConsumer(std::vector<ClangDiagnostic>& diagnostics) : diagnostics_(diagnostics)
            {
            }

            void HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic& diagnostic) override
            {
                clang::DiagnosticConsumer::HandleDiagnostic(level, diagnostic);

                std::filesystem::path source_file;
                std::uint32_t line = 0;
                std::uint32_t column = 0;

                if (diagnostic.hasSourceManager())
                {
                    const std::optional<SourcePosition> source_position =
                        GetSourcePosition(diagnostic.getSourceManager(), diagnostic.getLocation());
                    if (source_position.has_value())
                    {
                        source_file = source_position->source_file;
                        line = source_position->line;
                        column = source_position->column;
                    }
                }

                llvm::SmallString<256> formatted_message;
                diagnostic.FormatDiagnostic(formatted_message);

                diagnostics_.push_back(
                    ClangDiagnostic{ConvertDiagnosticSeverity(level), std::move(source_file), line, column,
                                    std::string{formatted_message.data(), formatted_message.size()}});
            }

        private:
            std::vector<ClangDiagnostic>& diagnostics_;
        };
    } // namespace

    ClangHeaderScanResult ScanHeaderWithClang(const HeaderParseInvocation& invocation)
    {
        if (invocation.arguments.empty())
        {
            return ClangHeaderScanError{
                ClangHeaderScanErrorCode::EmptyArguments, invocation.job.module_name, invocation.job.header_path, {}};
        }

        if (!invocation.job.header_path.is_absolute())
        {
            return ClangHeaderScanError{ClangHeaderScanErrorCode::HeaderPathNotAbsolute,
                                        invocation.job.module_name,
                                        invocation.job.header_path,
                                        {}};
        }

        if (!invocation.working_directory.is_absolute())
        {
            return ClangHeaderScanError{ClangHeaderScanErrorCode::WorkingDirectoryNotAbsolute,
                                        invocation.job.module_name,
                                        invocation.job.header_path,
                                        {}};
        }

        std::vector<ClangRecordDeclaration> records;
        std::vector<ClangEnumDeclaration> enums;
        std::vector<ClangDiagnostic> diagnostics;
        bool consumer_created = false;

        clang::FileSystemOptions file_system_options;
        file_system_options.WorkingDir = Utf8String(invocation.working_directory);
        llvm::IntrusiveRefCntPtr<clang::FileManager> file_manager =
            llvm::makeIntrusiveRefCnt<clang::FileManager>(file_system_options);

        auto frontend_action = std::make_unique<HeaderFrontendAction>(records, enums, consumer_created);
        CollectingDiagnosticConsumer diagnostic_consumer(diagnostics);

        std::vector<std::string> tool_arguments = PrepareToolArguments(invocation);

        clang::tooling::ToolInvocation tool_invocation(std::move(tool_arguments), std::move(frontend_action),
                                                       file_manager.get());
        tool_invocation.setDiagnosticConsumer(&diagnostic_consumer);

        if (!tool_invocation.run())
        {
            return ClangHeaderScanError{consumer_created ? ClangHeaderScanErrorCode::ToolExecutionFailed
                                                         : ClangHeaderScanErrorCode::TranslationUnitCreationFailed,
                                        invocation.job.module_name, invocation.job.header_path, std::move(diagnostics)};
        }

        return ClangHeaderScanOutput{invocation.job.module_name, invocation.job.header_path, std::move(records),
                                     std::move(diagnostics), std::move(enums)};
    }

} // namespace GE::Reflection::Generator
