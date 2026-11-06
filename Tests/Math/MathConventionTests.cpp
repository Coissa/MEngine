#include <Core/Math/math.h>

#include <cmath>
#include <iostream>
#include <type_traits>

namespace
{
    [[nodiscard]] bool Near(float left, float right, float epsilon = 0.0001F) noexcept
    {
        return std::abs(left - right) <= epsilon;
    }

    [[nodiscard]] int Fail(int code, const char* message)
    {
        std::cerr << "[MathConventionTests] " << message << '\n';
        return code;
    }
} // namespace
int main()
{
    using namespace GE::Math;

    static_assert(MatrixStorageConvention == MatrixStorage::RowMajor);
    static_assert(VectorMultiplicationConvention == VectorConvention::RowVector);
    static_assert(HandednessConvention == CoordinateHandedness::LeftHanded);
    static_assert(ClipSpaceDepthConvention == ClipSpaceDepth::ZeroToOne);
    static_assert(ShaderMatrixUploadConvention == ShaderMatrixUpload::NoTranspose);
    static_assert(sizeof(Vec3) == sizeof(float) * 3);
    static_assert(sizeof(Mat4) == sizeof(float) * 16);
    static_assert(std::is_trivially_copyable_v<Mat4>);

    if (!Near(ToRadians(Degrees{180}).Value(), Pi) || !Near(ToDegrees(Radians{Pi * 0.5F}).Value(), 90.0F))
    {
        return Fail(1, "explicit angle conversion changed");
    }

    const Vec3 cross = Cross({1, 0, 0}, {0, 1, 0});
    Vec3 normalized{3, 0, 4};
    Vec3 zero{};
    if (cross != Vec3{0, 0, 1} || !Normalize(normalized) || !Near(Length(normalized), 1.0F) || Normalize(zero))
    {
        return Fail(2, "vector operations are invalid");
    }

    const Mat4 translation = TranslationMatrix({5, 6, 7});
    if (translation[12] != 5 || translation[13] != 6 || translation[14] != 7)
    {
        return Fail(3, "row-major translation layout changed");
    }

    const Mat4 scale_then_translate = ScaleMatrix({2, 3, 4}) * translation;
    const Vec3 point = TransformPoint({1, 1, 1}, scale_then_translate);
    if (point != Vec3{7, 9, 11})
    {
        return Fail(4, "row-vector composition order changed");
    }

    const Mat4 yaw = RotationYMatrix(ToRadians(Degrees{90}));
    const Vec3 forward = TransformVector({0, 0, 1}, yaw);
    if (!Near(forward[0], 1) || !Near(forward[1], 0) || !Near(forward[2], 0))
    {
        return Fail(5, "left-handed +Z forward convention changed");
    }

    constexpr float NearPlane = 0.1F;
    constexpr float FarPlane = 100.0F;
    const Mat4 projection = PerspectiveFovLH_ZO(ToRadians(Degrees{60}), 16.0F / 9.0F, NearPlane, FarPlane);
    const Vec4 near_clip = Transform({0, 0, NearPlane, 1}, projection);
    const Vec4 far_clip = Transform({0, 0, FarPlane, 1}, projection);
    if (!Near(near_clip[2] / near_clip[3], 0) || !Near(far_clip[2] / far_clip[3], 1))
    {
        return Fail(6, "D3D zero-to-one clip depth changed");
    }

    const Quaternion quaternion = QuaternionFromAxisAngle({0, 1, 0}, ToRadians(Degrees{90}));
    const Vec3 quaternion_forward = TransformVector({0, 0, 1}, RotationMatrix(quaternion));
    if (!Near(quaternion_forward[0], 1) || !Near(quaternion_forward[2], 0))
    {
        return Fail(7, "quaternion rotation disagrees with matrix convention");
    }

    Mat4 normal_matrix;
    const Mat4 non_uniform_world = ScaleMatrix({2, 3, 4}) * RotationYMatrix(ToRadians(Degrees{35}));
    if (!TryMakeNormalMatrix(non_uniform_world, normal_matrix))
    {
        return Fail(8, "non-uniform scale normal matrix was rejected");
    }
    const Vec3 transformed_tangent = TransformVector({1, 1, 0}, non_uniform_world);
    Vec3 transformed_normal = TransformVector({1, -1, 0}, normal_matrix);
    if (!Normalize(transformed_normal) || !Near(Dot(transformed_tangent, transformed_normal), 0.0F))
    {
        return Fail(9, "inverse-transpose normal strategy is incorrect");
    }
    if (TryMakeNormalMatrix(ScaleMatrix({1, 0, 1}), normal_matrix))
    {
        return Fail(10, "singular normal transform was accepted");
    }

    float intersection = 0.0F;
    if (!IntersectRayAabb({{0, 0, -5}, {0, 0, 1}}, {{-1, -1, -1}, {1, 1, 1}}, 0, 100, intersection) ||
        !Near(intersection, 4.0F))
    {
        return Fail(11, "ray/AABB intersection is invalid");
    }

    std::cout << "[MathConventionTests] Passed\n";
    return 0;
}
