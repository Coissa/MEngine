#pragma once

#include "runtime/core/math/math.h"
#include "runtime/core/meta/reflection/reflection.h"

#include <cassert>

//四元数

namespace GE
{
    class Matrix3x3;
    class Vector3;
    class Quaternion
    {
        
    public:
        float x, y, z, w;

        Quaternion() : x(0), y(0), z(0), w(1) {}
        Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

        
    };
} // namespace GE