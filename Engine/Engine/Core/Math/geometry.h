#pragma once

#include "vector.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace GE::Math
{
    struct Ray3 final
    {
        Vec3 origin{};
        Vec3 direction{0, 0, 1};
    };

    struct Aabb3 final
    {
        Vec3 minimum{};
        Vec3 maximum{};

        [[nodiscard]] bool IsValid() const noexcept
        {
            return IsFinite(minimum) && IsFinite(maximum)
                && minimum[0] <= maximum[0]
                && minimum[1] <= maximum[1]
                && minimum[2] <= maximum[2];
        }
    };

    [[nodiscard]] inline bool IntersectRayAabb(
        const Ray3& ray,
        const Aabb3& bounds,
        float minimum_t,
        float maximum_t,
        float& out_t) noexcept
    {
        if (!IsFinite(ray.origin) || !IsFinite(ray.direction) || !bounds.IsValid()
            || !std::isfinite(minimum_t) || !std::isfinite(maximum_t)
            || minimum_t > maximum_t)
        {
            return false;
        }

        float enter = minimum_t;
        float exit = maximum_t;
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            if (ray.direction[axis] == 0.0F)
            {
                if (ray.origin[axis] < bounds.minimum[axis]
                    || ray.origin[axis] > bounds.maximum[axis])
                {
                    return false;
                }
                continue;
            }

            float first = (bounds.minimum[axis] - ray.origin[axis]) / ray.direction[axis];
            float second = (bounds.maximum[axis] - ray.origin[axis]) / ray.direction[axis];
            if (first > second) { std::swap(first, second); }
            enter = (std::max)(enter, first);
            exit = (std::min)(exit, second);
            if (enter > exit) { return false; }
        }
        out_t = enter;
        return std::isfinite(out_t);
    }
}
