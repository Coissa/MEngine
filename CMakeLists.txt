cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0184)
    cmake_policy(SET CMP0184 NEW)
endif()

project(MEngine LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT MEngineEditor)
set(CMAKE_FOLDER Tools)

option(BUILD_TESTING "Build the local test tree" OFF)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

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

# LLVM's packaged libraries use the static release CRT. When its tests link Engine,
# every executable consuming that Engine must use the same runtime library.
if(MSVC AND MENGINE_BUILD_REFLECTION_GENERATOR_TESTS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
endif()

add_subdirectory(Source)

if(MENGINE_BUILD_REFLECTION_GENERATOR_CLANG
    OR MENGINE_BUILD_REFLECTION_GENERATOR_TESTS
    OR MENGINE_ENABLE_REFLECTION_CODEGEN)
    add_library(MEngineReflectionGeneratorCore STATIC
        Tools/ReflectionGenerator/CompilationDatabase.cpp
        Tools/ReflectionGenerator/GeneratedFilePlan.cpp
        Tools/ReflectionGenerator/GeneratedFileWriter.cpp
        Tools/ReflectionGenerator/GenerationPlan.cpp
        Tools/ReflectionGenerator/HeaderParseInvocationBuilder.cpp
        Tools/ReflectionGenerator/HeaderScanJobBuilder.cpp
        Tools/ReflectionGenerator/HeaderScanJobResolver.cpp
        Tools/ReflectionGenerator/HeaderScanJobValidation.cpp
        Tools/ReflectionGenerator/ModuleManifestLoader.cpp
    )

    target_include_directories(MEngineReflectionGeneratorCore
        PUBLIC
            Tools/ReflectionGenerator
        PRIVATE
            ThirdParty
    )
endif()

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
        Tools/ReflectionGenerator/Clang/ClangDeclarationValidation.cpp
        Tools/ReflectionGenerator/Clang/ClangHeaderScanner.cpp
        Tools/ReflectionGenerator/Clang/ReflectionGenerationPipeline.cpp
        Tools/ReflectionGenerator/Clang/ReflectionCodeRenderer.cpp
        Tools/ReflectionGenerator/Clang/ReflectionModuleRenderer.cpp
    )

    if(MSVC)
        set_property(
            TARGET MEngineReflectionGeneratorClang
            PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded
        )
    endif()

    target_include_directories(MEngineReflectionGeneratorClang
        PUBLIC
            Tools/ReflectionGenerator
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
        Tools/ReflectionGenerator/ReflectionGeneratorMain.cpp
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
    set(reflection_manifest "${PROJECT_SOURCE_DIR}/Tools/ReflectionGenerator/Manifests/Runtime.Reflection.json")
    file(READ "${reflection_manifest}" reflection_manifest_json)
    string(JSON reflection_output GET "${reflection_manifest_json}" output_directory)
    string(JSON reflection_module GET "${reflection_manifest_json}" module_name)
    string(JSON reflection_header_count LENGTH "${reflection_manifest_json}" header_files)
    math(EXPR reflection_header_last "${reflection_header_count} - 1")
    set(reflection_generated_root "${PROJECT_SOURCE_DIR}/Source/${reflection_output}")
    set(reflection_outputs
        "${reflection_generated_root}/${reflection_module}.reflection.module.generated.h"
        "${reflection_generated_root}/${reflection_module}.reflection.module.generated.cpp")
    set(reflection_inputs "${reflection_manifest}")
    foreach(index RANGE ${reflection_header_last})
        string(JSON header GET "${reflection_manifest_json}" header_files ${index})
        list(APPEND reflection_inputs "${PROJECT_SOURCE_DIR}/Source/${header}")
        list(APPEND reflection_outputs
            "${reflection_generated_root}/${header}.reflection.generated.h"
            "${reflection_generated_root}/${header}.reflection.generated.cpp")
    endforeach()
    add_custom_command(OUTPUT ${reflection_outputs}
        COMMAND $<TARGET_FILE:MEngineReflectionGenerator>
            --compile-commands "${MENGINE_REFLECTION_COMPILATION_DATABASE}"
            --manifest "${reflection_manifest}"
        DEPENDS MEngineReflectionGenerator ${reflection_inputs}
            "${MENGINE_REFLECTION_COMPILATION_DATABASE}"
            "${PROJECT_SOURCE_DIR}/Source/Core/Reflection/ReflectionMacros.h"
            "${PROJECT_SOURCE_DIR}/Source/Core/Reflection/TypeTraits.h"
            "${PROJECT_SOURCE_DIR}/Source/Core/Serialization/BuiltinSerialization.h"
            "${PROJECT_SOURCE_DIR}/Source/Core/Math/vector.h"
        COMMENT "Generating runtime reflection and component implementations"
        VERBATIM)
    set_source_files_properties(${reflection_outputs} PROPERTIES GENERATED TRUE)
    add_custom_target(MEngineGenerateReflection DEPENDS ${reflection_outputs})
    add_dependencies(Engine MEngineGenerateReflection)
    target_sources(Engine PRIVATE ${reflection_outputs})
endif()

if(MENGINE_BUILD_REFLECTION_GENERATOR_TESTS)
    enable_testing()

    add_executable(MEngineReflectionGeneratorTests
        Tests/ReflectionGenerator/ClangReflectionGeneratorTests.cpp
        Tests/ReflectionGenerator/Fixtures/ClangRenderInput.h.reflection.generated.cpp
    )

    if(MSVC)
        set_property(
            TARGET Engine
            PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded
        )

        set_property(
            TARGET MEngineReflectionGeneratorTests
            PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded
        )
    endif()

    target_link_libraries(MEngineReflectionGeneratorTests
        PRIVATE
            Engine
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
            MENGINE_TEST_MANIFEST="${CMAKE_CURRENT_SOURCE_DIR}/Tools/ReflectionGenerator/Manifests/Core.Reflection.json"
            MENGINE_TEST_RENDER_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/Tests/ReflectionGenerator/Fixtures/ClangRenderInput.h"
            MENGINE_TEST_RENDER_GOLDEN_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/Tests/ReflectionGenerator/Fixtures/ClangRenderInput.h.reflection.generated.h"
            MENGINE_TEST_RENDER_GOLDEN_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/Tests/ReflectionGenerator/Fixtures/ClangRenderInput.h.reflection.generated.cpp"
            MENGINE_TEST_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
            MENGINE_TEST_SCAN_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/Tests/ReflectionGenerator/Fixtures/ClangScanInput.h"
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
        Tools/ReflectionGenerator/Smoke/ClangToolingSmoke.cpp
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

if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()
