#pragma once

#include "Math.hpp"

#include <array>
#include <cmath>

namespace glmath {

struct Mat4
{
  std::array<float, 16> m {};

  const float* Data() const
  {
    return m.data();
  }
};

inline Mat4 Identity()
{
  return {{
      1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f,
  }};
}

inline Mat4 Multiply(const Mat4& a, const Mat4& b)
{
  Mat4 out = {};
  for (int col = 0; col < 4; ++col)
  {
    for (int row = 0; row < 4; ++row)
    {
      float sum = 0.0f;
      for (int k = 0; k < 4; ++k)
      {
        sum += a.m[k * 4 + row] * b.m[col * 4 + k];
      }
      out.m[col * 4 + row] = sum;
    }
  }
  return out;
}

inline Mat4 operator*(const Mat4& a, const Mat4& b)
{
  return Multiply(a, b);
}

inline Mat4 Translation(float x, float y, float z)
{
  Mat4 out = Identity();
  out.m[12] = x;
  out.m[13] = y;
  out.m[14] = z;
  return out;
}

inline Mat4 Scale(float x, float y, float z)
{
  Mat4 out = Identity();
  out.m[0] = x;
  out.m[5] = y;
  out.m[10] = z;
  return out;
}

inline Mat4 RotationY(float degrees)
{
  const float radians = degrees * static_cast<float>(acos(-1.0) / 180.0);
  const float c = std::cos(radians);
  const float s = std::sin(radians);

  return {{
      c,    0.0f, s,    0.0f,
      0.0f, 1.0f, 0.0f, 0.0f,
      -s,   0.0f, c,    0.0f,
      0.0f, 0.0f, 0.0f, 1.0f,
  }};
}

inline Mat4 PerspectiveRH(float fovDegrees, float aspect, float nearPlane,
                          float farPlane)
{
  const float radians = fovDegrees * static_cast<float>(acos(-1.0) / 180.0);
  const float f = 1.0f / std::tan(radians * 0.5f);

  return {{
      f / aspect, 0.0f, 0.0f, 0.0f,
      0.0f, f, 0.0f, 0.0f,
      0.0f, 0.0f, (farPlane + nearPlane) / (nearPlane - farPlane), -1.0f,
      0.0f, 0.0f, (2.0f * farPlane * nearPlane) / (nearPlane - farPlane), 0.0f,
  }};
}

inline Mat4 LookAtRH(const Vector3& eye, const Vector3& at, const Vector3& up)
{
  const Vector3 forward = (eye - at).Normalize();
  const Vector3 right = math::CrossProduct(up, forward).Normalize();
  const Vector3 cameraUp = math::CrossProduct(forward, right);

  return {{
      right.x, cameraUp.x, forward.x, 0.0f,
      right.y, cameraUp.y, forward.y, 0.0f,
      right.z, cameraUp.z, forward.z, 0.0f,
      -math::DotProduct(right, eye),
      -math::DotProduct(cameraUp, eye),
      -math::DotProduct(forward, eye),
      1.0f,
  }};
}

}  // namespace glmath
