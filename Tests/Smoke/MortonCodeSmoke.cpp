#include <Renderer/Nanite/ClusterBuilder/MortonCode.h>

#include <cstdint>
#include <iostream>
#include <limits>

namespace
{
    [[nodiscard]] int Fail(int exit_code, const char* message)
    {
        std::cerr << "[MortonCodeSmoke] Failure: " << message << '\n';
        return exit_code;
    }
} // namespace

int main()
{
    using GE::Math::Vec3;
    using GE::Render::Nanite::CalculateMortonCode;

    if (CalculateMortonCode(Vec3{0.0F, 0.0F, 0.0F}) != 0U)
    {
        return Fail(1, "the origin did not map to zero");
    }
    if (CalculateMortonCode(Vec3{1.0F / 1024.0F, 0.0F, 0.0F}) != 1U ||
        CalculateMortonCode(Vec3{0.0F, 1.0F / 1024.0F, 0.0F}) != 2U ||
        CalculateMortonCode(Vec3{0.0F, 0.0F, 1.0F / 1024.0F}) != 4U)
    {
        return Fail(2, "axis bits were not interleaved as x, y, z");
    }
    if (CalculateMortonCode(Vec3{1.0F, 1.0F, 1.0F}) != 0x3FFFFFFFU)
    {
        return Fail(3, "the upper unit-cube corner did not use all 30 bits");
    }
    if (CalculateMortonCode(Vec3{-2.0F, 2.0F, 0.0F}) != CalculateMortonCode(Vec3{0.0F, 1.0F, 0.0F}))
    {
        return Fail(4, "coordinates outside the unit cube were not clamped");
    }
    if (CalculateMortonCode(Vec3{(std::numeric_limits<float>::quiet_NaN)(), 0.0F, 0.0F}) != 0U)
    {
        return Fail(5, "a non-finite position did not return zero");
    }

    std::cout << "[MortonCodeSmoke] Passed\n";
    return 0;
}
