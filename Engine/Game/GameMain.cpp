#include <Engine/Launch/EngineLoop.h>
#include <Engine/Runtime/GameEngine.h>

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
            std::make_unique<GE::Runtime::GameEngine>(),
            run_settings
        );
        engine_loop.Shutdown();
        return exit_code;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[MEngineGame] Unhandled exception: "
                  << exception.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "[MEngineGame] Unknown exception\n";
        return 1;
    }
}
