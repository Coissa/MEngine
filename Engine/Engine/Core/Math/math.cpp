#include "Engine/Core/Math/math.h"

namespace GE
{
    // Math 静态成员定义，不是函数。
    // 保存 Angle 使用的默认角度单位。
    Math::AngleUnit Math::k_AngleUnit;


    // ============================================================
    // Radian 与 Degree 转换
    // ============================================================

    // 使用角度值构造弧度对象。
    Radian::Radian(const Degree& degree)
    {
        // TODO
    }

    // 将角度值转换为弧度并赋给当前对象。
    Radian& Radian::operator=(const Degree& degree)
    {
        // TODO
    }

    // 将一个角度值加到当前弧度值，结果使用弧度表示。
    Radian Radian::operator+(const Degree& other) const
    {
        // TODO
    }

    // 从当前弧度值中减去一个角度值。
    Radian Radian::operator-(const Degree& other) const
    {
        // TODO
    }

    // 将角度值转换为弧度后累加到当前对象。
    Radian& Radian::operator+=(const Degree& other)
    {
        // TODO
    }

    // 将角度值转换为弧度后从当前对象中减去。
    Radian& Radian::operator-=(const Degree& other)
    {
        // TODO
    }


    // ============================================================
    // Degree 与 Radian 转换
    // ============================================================

    // 使用弧度值构造角度对象。
    Degree::Degree(const Radian& radian)
    {
        // TODO
    }

    // 将弧度值转换为角度并赋给当前对象。
    Degree& Degree::operator=(const Radian& radian)
    {
        // TODO
    }

    // 将一个弧度值加到当前角度值，结果使用角度表示。
    Degree Degree::operator+(const Radian& other) const
    {
        // TODO
    }

    // 从当前角度值中减去一个弧度值。
    Degree Degree::operator-(const Radian& other) const
    {
        // TODO
    }

    // 将弧度值转换为角度后累加到当前对象。
    Degree& Degree::operator+=(const Radian& other)
    {
        // TODO
    }

    // 将弧度值转换为角度后从当前对象中减去。
    Degree& Degree::operator-=(const Radian& other)
    {
        // TODO
    }


    // ============================================================
    // 数值设置
    // ============================================================

    // 直接设置内部弧度值。
    void Radian::setValue(float radians)
    {
        // TODO
    }

    // 直接设置内部角度值。
    void Degree::setValue(float degrees)
    {
        // TODO
    }


    // ============================================================
    // 默认角度单位转换
    // ============================================================

    // 将默认角度单位转换为弧度。
    float Math::angleUnitsToRadians(float angle_units)
    {
        // TODO
    }

    // 将默认角度单位转换为角度。
    float Math::angleUnitsToDegrees(float angle_units)
    {
        // TODO
    }

    // 将弧度转换为默认角度单位。
    float Math::radiansToAngleUnits(float radians)
    {
        // TODO
    }

    // 将角度转换为默认角度单位。
    float Math::degreesToAngleUnits(float degrees)
    {
        // TODO
    }


    // ============================================================
    // 矩阵生成
    // 等 Vector3、Matrix3x3、Matrix4x4、Quaternion 完成后实现
    // ============================================================

    // 根据世界位置和旋转方向生成观察矩阵。
    // reflect_matrix 不为空时，将反射变换应用到观察矩阵。
    Matrix4x4 Math::makeViewMatrix(
        const Vector3& position,
        const Quaternion& orientation,
        const Matrix4x4* reflect_matrix)
    {
        // TODO
    }

    // 根据摄像机位置、观察目标和向上方向生成 LookAt 观察矩阵。
    Matrix4x4 Math::makeLookAtMatrix(
        const Vector3& eye_position,
        const Vector3& target_position,
        const Vector3& up_direction)
    {
        // TODO
    }

    // 根据垂直视场角、宽高比、近平面和远平面生成透视投影矩阵。
    Matrix4x4 Math::makePerspectiveMatrix(
        Radian fovy,
        float aspect,
        float znear,
        float zfar)
    {
        // TODO
    }

    // 生成深度范围通常为 [-1, 1] 的正交投影矩阵。
    Matrix4x4 Math::makeOrthographicProjectionMatrix(
        float left,
        float right,
        float bottom,
        float top,
        float znear,
        float zfar)
    {
        // TODO
    }

    // 生成深度范围为 [0, 1] 的正交投影矩阵。
    Matrix4x4 Math::makeOrthographicProjectionMatrix01(
        float left,
        float right,
        float bottom,
        float top,
        float znear,
        float zfar)
    {
        // TODO
    }
} // namespace GE