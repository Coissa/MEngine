#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
    class SmokeRecordVisitor final
        : public clang::RecursiveASTVisitor<SmokeRecordVisitor>
    {
    public:
        bool VisitCXXRecordDecl(clang::CXXRecordDecl* declaration)
        {
            if (declaration->isThisDeclarationADefinition()
                && declaration->getName() == "MEngineSmokeType")
            {
                found_expected_type = true;
                std::cout << declaration->getQualifiedNameAsString() << '\n';
            }

            return true;
        }

        bool found_expected_type {false};
    };
}

int main()
{
    std::unique_ptr<clang::ASTUnit> ast = clang::tooling::buildASTFromCodeWithArgs(
        "namespace GE::Reflection::Generator { struct MEngineSmokeType {}; }",
        std::vector<std::string>{"-std=c++20"},
        "MEngineReflectionClangSmoke.cpp");

    if (!ast)
    {
        std::cerr << "Clang failed to build the smoke-test AST.\n";
        return 1;
    }

    SmokeRecordVisitor visitor;
    visitor.TraverseDecl(ast->getASTContext().getTranslationUnitDecl());

    if (!visitor.found_expected_type)
    {
        std::cerr << "The expected smoke-test type was not found.\n";
        return 2;
    }

    return 0;
}
