cmake_minimum_required(VERSION 3.20)

project(MEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(
    MENGINE_BUILD_REFLECTION_CLANG_SMOKE
    "Build the standalone Clang LibTooling integration smoke test"
    OFF
)

option(
    MENGINE_BUILD_REFLECTION_GENERATOR_CLANG
    "Build the Clang-backed reflection generator library"
    OFF
)

option(
    MENGINE_BUILD_REFLECTION_GENERATOR_TESTS
    "Build the Clang-backed reflection generator tests"
    OFF
)

option(
    MENGINE_ENABLE_REFLECTION_CODEGEN
    "Generate reflection sources from explicit module manifests during the build"
    OFF
)

set(
    MENGINE_REFLECTION_COMPILATION_DATABASE
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    CACHE FILEPATH
    "Compilation database consumed by the reflection generator"
)

add_library(MEngineReflection STATIC
    Engine/Engine/Core/Reflection/BuiltinTypeRegistration.cpp
    Engine/Engine/Core/Reflection/TypeRegistry.cpp
)

target_include_directories(MEngineReflection
    PUBLIC
        Engine/Engine/Core/Reflection
)

add_library(MEngineReflectionGeneratorCore STATIC
    Engine/Tools/ReflectionGenerator/CompilationDatabase.cpp
    Engine/Tools/ReflectionGenerator/GeneratedFilePlan.cpp
    Engine/Tools/ReflectionGenerator/GeneratedFileWriter.cpp
    Engine/Tools/ReflectionGenerator/GenerationPlan.cpp
    Engine/Tools/ReflectionGenerator/HeaderParseInvocationBuilder.cpp
    Engine/Tools/ReflectionGenerator/HeaderScanJobBuilder.cpp
    Engine/Tools/ReflectionGenerator/HeaderScanJobResolver.cpp
    Engine/Tools/ReflectionGenerator/HeaderScanJobValidation.cpp
    Engine/Tools/ReflectionGenerator/ModuleManifestLoader.cpp
)

target_include_directories(MEngineReflectionGeneratorCore
    PUBLIC
        Engine/Tools/ReflectionGenerator
    PRIVATE
        Engine/ThirdParty
)

if(MENGINE_BUILD_REFLECTION_GENERATOR_CLANG
    OR MENGINE_BUILD_REFLECTION_CLANG_SMOKE
    OR MENGINE_BUILD_REFLECTION_GENERATOR_TESTS
    OR MENGINE_ENABLE_REFLECTION_CODEGEN)
    find_package(LLVM 22.1 CONFIG REQUIRED)
    find_package(Clang 22.1 CONFIG REQUIRED)

    if(MSVC AND TARGET LLVMDebugInfoPDB)
        get_target_property(
            llvm_debug_info_pdb_link_libraries
            LLVMDebugInfoPDB
            INTERFACE_LINK_LIBRARIES
        )

        set(llvm_debug_info_pdb_fixed_link_libraries "")
        foreach(link_library IN LISTS llvm_debug_info_pdb_link_libraries)
            if(link_library MATCHES "[/\\\\]DIA SDK[/\\\\]lib[/\\\\]amd64[/\\\\]diaguids\\.lib$"
                AND NOT EXISTS "${link_library}")
                find_library(
                    MENGINE_DIA_GUIDS_LIBRARY
                    NAMES diaguids
                    HINTS
                        "$ENV{VSINSTALLDIR}/DIA SDK/lib/amd64"
                    REQUIRED
                )
                list(APPEND llvm_debug_info_pdb_fixed_link_libraries
                    "${MENGINE_DIA_GUIDS_LIBRARY}"
                )
            else()
                list(APPEND llvm_debug_info_pdb_fixed_link_libraries
                    "${link_library}"
                )
            endif()
        endforeach()

        set_property(
            TARGET LLVMDebugInfoPDB
            PROPERTY INTERFACE_LINK_LIBRARIES
                "${llvm_debug_info_pdb_fixed_link_libraries}"
        )
    endif()
endif()

if(MENGINE_BUILD_REFLECTION_GENERATOR_CLANG
    OR MENGINE_BUILD_REFLECTION_GENERATOR_TESTS
    OR MENGINE_ENABLE_REFLECTION_CODEGEN)
    if(MSVC)
        set_property(
            TARGET MEngineReflectionGeneratorCore
            PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded
        )
    endif()

    add_library(MEngineReflectionGeneratorClang STATIC
        Engine/Tools/ReflectionGenerator/Clang/ClangDeclarationValidation.cpp
        Engine/Tools/ReflectionGenerator/Clang/ClangHeaderScanner.cpp
        Engine/Tools/ReflectionGenerator/Clang/ReflectionGenerationPipeline.cpp
        Engine/Tools/ReflectionGenerator/Clang/ReflectionCodeRenderer.cpp
        Engine/Tools/ReflectionGenerator/Clang/ReflectionModuleRenderer.cpp
    )

    if(MSVC)
        set_property(
            TARGET MEngineReflectionGeneratorClang
            PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded
        )
    endif()

    target_include_directories(MEngineReflectionGeneratorClang
        PUBLIC
            Engine/Tools/ReflectionGenerator
    )

    target_include_directories(MEngineReflectionGeneratorClang
        SYSTEM
        PRIVATE
            ${LLVM_INCLUDE_DIRS}
            ${CLANG_INCLUDE_DIRS}
    )

    target_link_libraries(MEngineReflectionGeneratorClang
        PUBLIC
            MEngineReflectionGeneratorCore
        PRIVATE
            clangTooling
    )

    add_executable(MEngineReflectionGenerator
        Engine/Tools/ReflectionGenerator/ReflectionGeneratorMain.cpp
    )

    if(MSVC)
        set_property(
            TARGET MEngineReflectionGenerator
            PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded
        )
    endif()

    target_link_libraries(MEngineReflectionGenerator
        PRIVATE
            MEngineReflectionGeneratorClang
    )
endif()

if(MENGINE_ENABLE_REFLECTION_CODEGEN)
    set(mengine_core_reflection_manifest
        "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Manifests/Core.Reflection.json"
    )
    set(mengine_core_reflection_input_header
        "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Engine/Core/Reflection/TypeRegistry.h"
    )
    set(mengine_core_reflection_compile_source
        "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Engine/Core/Reflection/TypeRegistry.cpp"
    )
    set(mengine_core_reflection_generated_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Generated/Reflection/Engine/Core/Reflection"
    )
    set(mengine_core_reflection_generated_header
        "${mengine_core_reflection_generated_directory}/TypeRegistry.h.reflection.generated.h"
    )
    set(mengine_core_reflection_generated_source
        "${mengine_core_reflection_generated_directory}/TypeRegistry.h.reflection.generated.cpp"
    )
    set(mengine_core_reflection_module_header
        "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Generated/Reflection/MEngine.Core.Reflection.reflection.module.generated.h"
    )
    set(mengine_core_reflection_module_source
        "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Generated/Reflection/MEngine.Core.Reflection.reflection.module.generated.cpp"
    )

    add_custom_command(
        OUTPUT
            "${mengine_core_reflection_generated_header}"
            "${mengine_core_reflection_generated_source}"
            "${mengine_core_reflection_module_header}"
            "${mengine_core_reflection_module_source}"
        COMMAND
            $<TARGET_FILE:MEngineReflectionGenerator>
            --compile-commands
            "${MENGINE_REFLECTION_COMPILATION_DATABASE}"
            --manifest
            "${mengine_core_reflection_manifest}"
        DEPENDS
            MEngineReflectionGenerator
            "${MENGINE_REFLECTION_COMPILATION_DATABASE}"
            "${mengine_core_reflection_manifest}"
            "${mengine_core_reflection_input_header}"
            "${mengine_core_reflection_compile_source}"
        COMMENT
            "Generating reflection sources for MEngine.Core.Reflection"
        VERBATIM
    )

    set_source_files_properties(
        "${mengine_core_reflection_generated_header}"
        "${mengine_core_reflection_generated_source}"
        "${mengine_core_reflection_module_header}"
        "${mengine_core_reflection_module_source}"
        PROPERTIES GENERATED TRUE
    )
    set_source_files_properties(
        "${mengine_core_reflection_generated_header}"
        "${mengine_core_reflection_module_header}"
        PROPERTIES HEADER_FILE_ONLY TRUE
    )

    add_custom_target(MEngineGenerateReflection
        DEPENDS
            "${mengine_core_reflection_generated_header}"
            "${mengine_core_reflection_generated_source}"
            "${mengine_core_reflection_module_header}"
            "${mengine_core_reflection_module_source}"
    )

    target_sources(MEngineReflection
        PRIVATE
            "${mengine_core_reflection_generated_header}"
            "${mengine_core_reflection_generated_source}"
            "${mengine_core_reflection_module_header}"
            "${mengine_core_reflection_module_source}"
    )
endif()

if(MENGINE_BUILD_REFLECTION_GENERATOR_TESTS)
    enable_testing()

    add_executable(MEngineReflectionGeneratorTests
        Engine/Tests/ReflectionGenerator/ClangReflectionGeneratorTests.cpp
        Engine/Tests/ReflectionGenerator/Fixtures/ClangRenderInput.h.reflection.generated.cpp
    )

    if(MSVC)
        set_property(
            TARGET MEngineReflection
            PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded
        )

        set_property(
            TARGET MEngineReflectionGeneratorTests
            PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded
        )
    endif()

    target_link_libraries(MEngineReflectionGeneratorTests
        PRIVATE
            MEngineReflection
            MEngineReflectionGeneratorClang
    )

    target_include_directories(MEngineReflectionGeneratorTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_definitions(MEngineReflectionGeneratorTests
        PRIVATE
            MENGINE_TEST_CLANG_EXECUTABLE="${LLVM_TOOLS_BINARY_DIR}/clang-cl.exe"
            MENGINE_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
            MENGINE_TEST_MANIFEST="${CMAKE_CURRENT_SOURCE_DIR}/Engine/Manifests/Core.Reflection.json"
            MENGINE_TEST_RENDER_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/Engine/Tests/ReflectionGenerator/Fixtures/ClangRenderInput.h"
            MENGINE_TEST_RENDER_GOLDEN_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/Engine/Tests/ReflectionGenerator/Fixtures/ClangRenderInput.h.reflection.generated.h"
            MENGINE_TEST_RENDER_GOLDEN_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/Engine/Tests/ReflectionGenerator/Fixtures/ClangRenderInput.h.reflection.generated.cpp"
            MENGINE_TEST_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
            MENGINE_TEST_SCAN_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/Engine/Tests/ReflectionGenerator/Fixtures/ClangScanInput.h"
    )

    add_test(
        NAME MEngineReflectionGeneratorTests
        COMMAND MEngineReflectionGeneratorTests
    )

    add_test(
        NAME MEngineReflectionGeneratorHelp
        COMMAND MEngineReflectionGenerator --help
    )
endif()

if(MENGINE_BUILD_REFLECTION_CLANG_SMOKE)
    add_executable(MEngineReflectionClangSmoke
        Engine/Tools/ReflectionGenerator/Smoke/ClangToolingSmoke.cpp
    )

    target_include_directories(MEngineReflectionClangSmoke
        SYSTEM
        PRIVATE
            ${LLVM_INCLUDE_DIRS}
            ${CLANG_INCLUDE_DIRS}
    )

    target_link_libraries(MEngineReflectionClangSmoke
        PRIVATE
            clangTooling
    )
endif()
