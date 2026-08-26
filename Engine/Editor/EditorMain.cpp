#include "EditorEngine.h"

#include <Engine/Launch/EngineLoop.h>

#include <exception>
#include <iostream>
#include <memory>

int main()
{
    try
    {
        GE::Launch::EngineLoop engine_loop;
        if (!engine_loop.Initialize())
        {
            return 1;
        }

        const GE::Launch::EngineRunSettings run_settings{
            .max_frames = 1
        };

        const int exit_code = engine_loop.Run(
            std::make_unique<GE::Editor::EditorEngine>(),
            run_settings
        );
        engine_loop.Shutdown();
        return exit_code;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[MEngineEditor] Unhandled exception: "
                  << exception.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "[MEngineEditor] Unknown exception\n";
        return 1;
    }
}
