#pragma once

namespace gx_math
{
template <class Vector>
inline Vector crossProduct(const Vector& left, const Vector& right)
{
    return Vector(left.y * right.z - left.z * right.y,
                  left.z * right.x - left.x * right.z,
                  left.x * right.y - left.y * right.x);
}
}
