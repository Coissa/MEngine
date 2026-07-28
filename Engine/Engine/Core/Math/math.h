//提供基础运算 数学常量等
#pragma once

#include "Engine/Core/Math/random.h"
#include <cfloat>
#include <algorithm>
#include <cmath>
#include <limits>

#define CMP(x, y) (fabsf(x - y) < FLT_EPSILON * fmaxf(1.0f, fmaxf(fabsf(x), fabsf(y))))
//判断浮点数近似相等的宏 暂时不知道有什么用
namespace GE
{
    inline constexpr float Math_POS_INFINITY = std::numeric_limits<float>::infinity();
    inline constexpr float Math_NEG_INFINITY = -std::numeric_limits<float>::infinity();
    inline constexpr float Math_PI          = 3.14159265358979323846f;
    inline constexpr float Math_ONE_OVER_PI = 1.0f / Math_PI;
    inline constexpr float Math_TWO_PI      = 2.0f * Math_PI;
    inline constexpr float Math_HALF_PI     = 0.5f * Math_PI;
    inline constexpr float Math_fDeg2Rad    = Math_PI / 180.0f;
    inline constexpr float Math_fRad2Deg    = 180.0f / Math_PI;
    inline constexpr float Math_LOG2        = 0.6931471805599453f;
    inline constexpr float Math_EPSILON     = 1e-6f;
    inline constexpr float Float_EPSILON    = std::numeric_limits<float>::epsilon();
    inline constexpr double Double_EPSILON  = std::numeric_limits<double>::epsilon();
    //piccolo采用的是static const 这里采用constexpr  constexpr是编译期常量 static const是运行期常量

    template<typename T>
    T clamp(T value, T min, T max)
    {
        return std::max(min, std::min(value, max));
    }

    template<typename T>
    T lerp(T a, T b, float t)
    {
        return a + (b - a) * t;
    }

    class Radian;
    class Angle;
    class Degree;
    class Vector2;
    class Vector3;
    class Vector4;
    class Matrix3x3;
    class Matrix4x4;
    class Quaternion;
    //类的前向声明

    class Radian
    {
        float m_rad { 0.0f };
    public:
        explicit Radian(float rad = 0.0f) : m_rad(rad) {}
        explicit Radian(const Degree& degree);
        Radian& operator=(float rad) { m_rad = rad; return *this; }
        Radian& operator=(const Degree& Degree); //角度转弧度

        //set和get
        float valueRadians() const { return m_rad; };
        float valueDegrees() const;
        float valueAngleUnits() const;
        void setValue(float rad);

        //算数符号
        Radian operator+(const Radian& other) const { return Radian(m_rad + other.m_rad); }
        Radian operator-(const Radian& other) const { return Radian(m_rad - other.m_rad); }
        Radian operator*(float scalar) const { return Radian(m_rad * scalar); }
        Radian operator/(float scalar) const { return Radian(m_rad / scalar); }
        //和Degree的算数符号
        Radian operator+(const Degree& other) const;
        Radian operator-(const Degree& other) const;

        //+=和-=运算符
        Radian& operator+=(const Radian& other)
        {
            m_rad += other.m_rad;
            return *this;
        }
        Radian& operator-=(const Radian& other)
        {
            m_rad -= other.m_rad;
            return *this;
        }
        Radian& operator+=(const Degree& other);
        Radian& operator-=(const Degree& other);
        Radian& operator*=(float scalar) { m_rad *= scalar; return *this; };
        Radian& operator/=(float scalar) { m_rad /= scalar; return *this; };

        //比较运算符
        bool operator==(const Radian& other) const { return CMP(m_rad, other.m_rad); }
        bool operator!=(const Radian& other) const { return !CMP(m_rad, other.m_rad); }
        bool operator<(const Radian& other) const { return m_rad < other.m_rad; }
        bool operator<=(const Radian& other) const { return m_rad <= other.m_rad; }
        bool operator>(const Radian& other) const { return m_rad > other.m_rad; }
        bool operator>=(const Radian& other) const { return m_rad >= other.m_rad; }
    };

    class Degree
    {
        float m_deg { 0.0f }; 
    public:
        explicit Degree(float deg = 0.0f) : m_deg(deg) {}
        explicit Degree(const Radian& rad);
        Degree& operator=(float deg) { m_deg = deg; return *this; }
        Degree& operator=(const Radian& rad);

        //set和get
        float valueDegrees() const { return m_deg; };
        float valueRadians() const;
        float valueAngleUnits() const;
        void setValue(float deg);

        //算数符号
        Degree operator+(const Degree& other) const { return Degree(m_deg + other.m_deg); }
        Degree operator-(const Degree& other) const { return Degree(m_deg - other.m_deg); }
        Degree operator*(float scalar) const { return Degree(m_deg * scalar); }
        Degree operator/(float scalar) const { return Degree(m_deg / scalar); }
        //和Radian的算数符号
        Degree operator+(const Radian& other) const { return Degree(m_deg + other.valueDegrees()); };
        Degree operator-(const Radian& other) const { return Degree(m_deg - other.valueDegrees()); };

        //+=和-=运算符
        Degree& operator+=(const Degree& other)
        {
            m_deg += other.m_deg;
            return *this;
        }
        Degree& operator-=(const Degree& other) 
        {
            m_deg -= other.m_deg;
            return *this;
        }
        Degree& operator+=(const Radian& other);
        Degree& operator-=(const Radian& other);
        Degree& operator*=(float scalar) { m_deg *= scalar; return *this; };
        Degree& operator/=(float scalar) { m_deg /= scalar; return *this; };

        //比较运算符
        bool operator==(const Degree& other) const { return CMP(m_deg, other.m_deg); }
        bool operator!=(const Degree& other) const { return !CMP(m_deg, other.m_deg); }
        bool operator<(const Degree& other) const { return m_deg < other.m_deg; }
        bool operator<=(const Degree& other) const { return m_deg <= other.m_deg; }
        bool operator>(const Degree& other) const { return m_deg > other.m_deg; }
        bool operator>=(const Degree& other) const { return m_deg >= other.m_deg; }
    };

    //默认角度
    class Angle
    {
        float m_angle { 0.0f };
    public:
        Angle() = default;
        explicit Angle(float angle) : m_angle(angle) {}
        
        explicit operator Radian() const;
        explicit operator Degree() const;
    };

    //
    class Math
    {
        enum class AngleUnit
        {
            AU_Degrees,
            AU_Radians
        };
        static AngleUnit k_AngleUnit;
    public:
        //基础数学函数
        static float abs(float value) { return std::fabs(value); }
        static bool isNan(float value) { return std::isnan(value); }
        static float sqr(float value) { return value * value; }
        static float sqrt(float value) { return std::sqrt(value); }
        static float invSqrt(float value) { return 1.0f / std::sqrt(value); }
        static bool realEqual(float a, float b, float tolerance = Math_EPSILON) { return std::fabs(a - b) <= tolerance; }
        static float clamp(float value, float min, float max) { return std::max(min, std::min(value, max)); }
        static float getMaxElement(float a, float b, float c) { return std::max(a, std::max(b, c)); }
        //泛型比较
        template<typename T>
        static constexpr T max(T a, T b) { return (a > b) ? a : b; }

        template<typename T>
        static constexpr T min(T a, T b) { return (a < b) ? a : b; }

        template<typename T>
        static constexpr T max3(T a, T b, T c) { return std::max({a, b, c}); }

        template<typename T>
        static constexpr T min3(T a, T b, T c) { return std::min({a, b, c}); }

        //弧度制角度制转换
        static float degreesToRadians(float degrees) { return degrees * Math_fDeg2Rad; }
        static float radiansToDegrees(float radians) { return radians * Math_fRad2Deg; }

        static float angleUnitsToRadians(float angleUnits);
        static float angleUnitsToDegrees(float angleUnits);
        
        static float radiansToAngleUnits(float radians);
        static float degreesToAngleUnits(float degrees);
        
        //三角函数
        static float sin(float angle) { return std::sin(angle); }
        static float cos(float angle) { return std::cos(angle); }   
        static float sin(Radian rad) { return std::sin(rad.valueRadians()); }
        static float cos(Radian rad) { return std::cos(rad.valueRadians()); }
        static float tan(float angle) { return std::tan(angle); }
        static float tan(Radian rad) { return std::tan(rad.valueRadians()); }
        static Radian asin(float value) { return Radian(std::asin(value)); }
        static Radian acos(float value) { return Radian(std::acos(value)); }
        static Radian atan(float value) { return Radian(std::atan(value)); }
        static Radian atan2(float y_v, float x_v) { return Radian(std::atan2(y_v, x_v)); }

        //矩阵运算
        static Matrix4x4 makeViewMatrix(const Vector3& position, const Quaternion& orientation, const Matrix4x4* reflect_matrix = nullptr);
        static Matrix4x4 makeLookAtMatrix(const Vector3& eye_position, const Vector3& target_position, const Vector3& up_dir);
        static Matrix4x4 makePerspectiveMatrix(Radian fovy, float aspect, float znear, float zfar);
        static Matrix4x4 makeOrthographicProjectionMatrix(float left, float right, float bottom, float top, float znear, float zfar);
        static Matrix4x4 makeOrthographicProjectionMatrix01(float left, float right, float bottom, float top, float znear, float zfar);
    };

    //补充函数
    inline float Radian::valueDegrees() const { return Math::radiansToDegrees(m_rad); }
    inline float Radian::valueAngleUnits() const { return Math::radiansToAngleUnits(m_rad); }
    inline float Degree::valueRadians() const { return Math::degreesToRadians(m_deg); }
    inline float Degree::valueAngleUnits() const { return Math::degreesToAngleUnits(m_deg); }
    inline Angle::operator Radian() const { return Radian(Math::angleUnitsToRadians(m_angle)); }
    inline Angle::operator Degree() const { return Degree(Math::angleUnitsToDegrees(m_angle)); }
    inline Radian operator*(float a, const Radian& b) { return Radian(a * b.valueRadians()); }
    inline Radian operator/(float a, const Radian& b) { return Radian(a / b.valueRadians()); }
    inline Degree operator*(float a, const Degree& b) { return Degree(a * b.valueDegrees()); }
    inline Degree operator/(float a, const Degree& b) { return Degree(a / b.valueDegrees()); }
}