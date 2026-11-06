#pragma once
#include <memory>
namespace GE
{
    namespace Editor
    {
        class EditorSession;
    }
    namespace Asset
    {
        class AssetRegistry;
    }
    namespace Reflection
    {
        class TypeRegistry;
    }
    class Engine
    {
    public:
        static Engine& Get();
        ~Engine();
        int Run(int argumentCount, char** arguments);
        void Shutdown() noexcept;
        Editor::EditorSession& Editor();
        Asset::AssetRegistry& Assets();
        Reflection::TypeRegistry& Reflection();

    private:
        Engine() = default;
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;
        struct State;
        std::unique_ptr<State> state;
    };
} // namespace GE
