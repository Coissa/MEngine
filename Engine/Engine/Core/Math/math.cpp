#include "Engine/Core/Math/math.h"

namespace GE
{
    // Math 静态成员定义，不是函数。
    // 保存 Angle 使用的默认角度单位。
    Math::AngleUnit Math::k_AngleUnit;

    Math::Math() { k_AngleUnit = AngleUnit::AU_DEGREE; } //默认角度
    // ============================================================
    // Radian 与 Degree 转换
    // ============================================================

    // 使用角度值构造弧度对象。
    Radian::Radian(const Degree& degree)
    {
        // TODO
        m_rad = degree.valueRadians();
    }

    // 将角度值转换为弧度并赋给当前对象。
    Radian& Radian::operator=(const Degree& degree)
    {
        // TODO
        m_rad = degree.valueRadians();
        return *this;
    }

    // 将一个角度值加到当前弧度值，结果使用弧度表示。
    Radian Radian::operator+(const Degree& other) const
    {
        // TODO
        m_rad += other.valueRadians();
        return *this;
    }

    // 从当前弧度值中减去一个角度值。
    Radian Radian::operator-(const Degree& other) const
    {
        m_rad -= other.valueRadians();
        return *this;
    }

    // 将角度值转换为弧度后累加到当前对象。
    Radian& Radian::operator+=(const Degree& other)
    {
        m_rad += other.valueRadians();
        return *this;
    }

    // 将角度值转换为弧度后从当前对象中减去。
    Radian& Radian::operator-=(const Degree& other)
    {
        m_rad -= other.valueRadians();
        return *this;
    }


    // Degree 与 Radian 转换

    // 使用弧度值构造角度对象。
    Degree::Degree(const Radian& radian)
    {
        m_deg = radian.valueDegrees();
    }

    // 将弧度值转换为角度并赋给当前对象。
    Degree& Degree::operator=(const Radian& radian)
    {
        m_deg = radian.valueDegrees();
        return *this;
    }

    // 将一个弧度值加到当前角度值，结果使用角度表示。
    Degree Degree::operator+(const Radian& other) const
    {
        m_deg += other.valueDegrees();
        return *this;
    }

    // 从当前角度值中减去一个弧度值。
    Degree Degree::operator-(const Radian& other) const
    {
        m_deg -= other.valueDegrees();
        return *this;
    }

    // 将弧度值转换为角度后累加到当前对象。
    Degree& Degree::operator+=(const Radian& other)
    {
        m_deg += other.valueDegrees();
        return *this;
    }

    // 将弧度值转换为角度后从当前对象中减去。
    Degree& Degree::operator-=(const Radian& other)
    {
        m_deg -= other.valueDegrees();
        return *this;
    }


    // ============================================================
    // 数值设置
    // ============================================================

    // 直接设置内部弧度值。
    void Radian::setValue(float radians)
    {
        m_rad = radians;
    }

    // 直接设置内部角度值。
    void Degree::setValue(float degrees)
    {
        m_deg = degrees;
    }


    // ============================================================
    // 默认角度单位转换
    // ============================================================

    // 将默认角度单位转换为弧度。
    float Math::angleUnitsToRadians(float angle_units)
    {
        if (k_AngleUnit == AngleUnit::AU_DEGREE)
        {
            return degreesToRadians(angle_units);
        }
        return angle_units; // 默认角度单位为弧度

    }

    // 将默认角度单位转换为角度。
    float Math::angleUnitsToDegrees(float angle_units)
    {
        if (k_AngleUnit == AngleUnit::AU_RADIANS)
        {
            return radiansToDegrees(angle_units);
        }
        return angle_units; // 默认角度单位为角度
    }

    // 将弧度转换为默认角度单位。
    float Math::radiansToAngleUnits(float radians)
    {
        if (k_AngleUnit == AngleUnit::AU_DEGREE)
        {
            return radiansToDegrees(radians);
        }
        return radians; // 默认角度单位为弧度
    }

    // 将角度转换为默认角度单位。
    float Math::degreesToAngleUnits(float degrees)
    {
        if (k_AngleUnit == AngleUnit::AU_RADIANS)
        {
            return degreesToRadians(degrees);
        }
        return degrees; // 默认角度单位为角度
    }


    // 根据世界位置和旋转方向生成观察矩阵。
    // reflect_matrix 不为空时，将反射变换应用到观察矩阵。
    Matrix4x4 Math::makeViewMatrix(
        const Vector3& position,
        const Quaternion& orientation,
        const Matrix4x4* reflect_matrix)
    {
        
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
        const float tan_half_fovy = Math::tan(fovy / 2.0f);
        const float inv_tan_half_fovy = 1.0f / tan_half_fovy;

        Matrix4x4 result = Matrix4x4::ZERO;
        result[0][0] = inv_tan_half_fovy / aspect;
        result[1][1] = inv_tan_half_fovy;
        result[2][2] = zfar / (znear - zfar);
        result[2][3] = -(zfar * znear) / (zfar - znear);
        result[3][2] = -1.0f;
        return result;
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
