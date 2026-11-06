#include <Engine.h>
#include <exception>
#include <iostream>
int main(int argc, char** argv)
{
    try
    {
        const int result = GE::Engine::Get().Run(argc, argv);
        GE::Engine::Get().Shutdown();
        return result;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[Editor] " << error.what() << '\n';
        GE::Engine::Get().Shutdown();
        return 1;
    }
}
