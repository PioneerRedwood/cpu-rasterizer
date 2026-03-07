//------------------------------------------------------------------------------
// File: Math.cpp
// Author: Chris Redwood
// Created: 2024-11-01
// License: MIT License
//------------------------------------------------------------------------------

#include "Math.hpp"

Vector2::Vector2() : x(0.0f), y(0.0f) {}

Vector2::Vector2(float x, float y) : x(x), y(y) {}

Vector2 Vector2::operator+(const Vector2& other) const {
  return { x + other.x, y + other.y };
}

Vector2 Vector2::operator*(float scalar) const {
  return { x * scalar, y * scalar };
}

Vector2& Vector2::operator=(const Vector2& other) {
  x = other.x;
  y = other.y;
  return *this;
}

Vector2 Vector2::operator-(const Vector2& other) const {
  return { x - other.x, y - other.y };
}

bool Vector2::operator==(const Vector2& other) const {
  return (x == other.x) && (y == other.y);
}

float Vector2::Magnitude() const {
  return sqrtf(x * x + y * y);
}

Vector2 Vector2::Normalized() const {
  float mag = Magnitude();
  return { x / mag, y / mag };
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Vector3::Vector3() : x(0.0f), y(0.0f), z(0.0f) {}

Vector3::Vector3(float x, float y, float z)
: x(x), y(y), z(z) {}

Vector3::Vector3(const Vector3& other)
: x(other.x), y(other.y), z(other.z) {}

Vector3& Vector3::operator=(const Vector3& other) {
  this->x = other.x;
  this->y = other.y;
  this->z = other.z;
  return *this;
}

Vector3 Vector3::operator+(const Vector3& other) const {
  return {this->x + other.x, this->y + other.y, this->z + other.z};
}

Vector3 Vector3::operator*(float scalar) const {
  return { this->x * scalar, this->y * scalar, this->z * scalar };
}

Vector3 Vector3::operator-(const Vector3& other) const {
  return { this->x - other.x, this->y - other.y, this->z - other.z };
}

Vector3 Vector3::operator/(const Vector3& other) const {
  return { this->x / other.x, this->y / other.y, this->z / other.z };
}

Vector3 Vector3::operator/(float value) const {
  return { this->x / value, this->y / value, this->z / value };
}

bool Vector3::operator==(const Vector3& other) const {
  return (this->x == other.x && this->y == other.y && this->z == other.z);
}

Vector3 Vector3::Normalize() const {
  const float mag = (float)std::sqrt(this->x * this->x + this->y * this->y +
                                     this->z * this->z);
  return (*this) / mag;
}

std::string Vector3::ToString() const {
  char buf[64];
  snprintf(buf, 64, "(%4.2f, %4.2f, %4.2f)", this->x, this->y, this->z);
  return std::string(buf);
}

namespace math {

Vector3 Subtract(const Vector3& v1, const Vector3& v2) {
  return v1 - v2;
}

float DotProduct(const Vector3& v1, const Vector3& v2) {
  return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

Vector3 CrossProduct(const Vector3& v1, const Vector3& v2) {
  return {
    v1.y * v2.z - v1.z * v2.y,
    v1.z * v2.x - v1.x * v2.z,
    v1.x * v2.y - v1.y * v2.x
  };
}

}

////////////////////////////////////////////////////////////////////////////////////////////////////

Vector4::Vector4()
: x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

Vector4::Vector4(float x, float y, float z, float w)
: x(x), y(y), z(z), w(w) {}

void Vector4::PerspectiveDivide() {
  const float rhw = 1 / w;
  this->x = x * rhw;
  this->y = y * rhw;
  this->z = z * rhw;
  this->w = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Matrix4x4 Matrix4x4::identity = {
  1, 0, 0, 0,
  0, 1, 0, 0,
  0, 0, 1, 0,
  0, 0, 0, 1
};

Matrix4x4::Matrix4x4()
: m11(0.0f), m12(0.0f), m13(0.0f), m14(0.0f),
  m21(0.0f), m22(0.0f), m23(0.0f), m24(0.0f),
  m31(0.0f), m32(0.0f), m33(0.0f), m34(0.0f),
  m41(0.0f), m42(0.0f), m43(0.0f), m44(0.0f) {}

Matrix4x4::Matrix4x4(
  float m11, float m12, float m13, float m14,
  float m21, float m22, float m23, float m24,
  float m31, float m32, float m33, float m34,
  float m41, float m42, float m43, float m44 )
: m11(m11), m12(m12), m13(m13), m14(m14),
  m21(m21), m22(m22), m23(m23), m24(m24),
  m31(m31), m32(m32), m33(m33), m34(m34),
  m41(m41), m42(m42), m43(m43), m44(m44) {}

Matrix4x4::Matrix4x4(Vector4 x, Vector4 y, Vector4 z, Vector4 w)
: m11(x.x), m12(y.x), m13(z.x), m14(w.x),
  m21(x.y), m22(y.y), m23(z.y), m24(w.y),
  m31(x.z), m32(y.z), m33(z.z), m34(w.z),
  m41(x.w), m42(y.w), m43(z.w), m44(w.w) {}

Matrix4x4::Matrix4x4(const Vector4& x, const Vector4& y, 
                     const Vector4& z, const Vector4& w)
: m11(x.x), m12(y.x), m13(z.x), m14(w.x),
  m21(x.y), m22(y.y), m23(z.y), m24(w.y),
  m31(x.z), m32(y.z), m33(z.z), m34(w.z),
  m41(x.w), m42(y.w), m43(z.w), m44(w.w) {}

Matrix4x4::Matrix4x4(const Matrix4x4& other) {
  this->m11 = other.m11, this->m12 = other.m12, this->m13 = other.m13, this->m14 = other.m14;
  this->m21 = other.m21, this->m22 = other.m22, this->m23 = other.m23, this->m24 = other.m24;
  this->m31 = other.m31, this->m32 = other.m32, this->m33 = other.m33, this->m34 = other.m34;
  this->m41 = other.m41, this->m42 = other.m42, this->m43 = other.m43, this->m44 = other.m44;
}

Matrix4x4& Matrix4x4::operator=(const Matrix4x4& other) {
  this->m11 = other.m11, this->m12 = other.m12, this->m13 = other.m13, this->m14 = other.m14;
  this->m21 = other.m21, this->m22 = other.m22, this->m23 = other.m23, this->m24 = other.m24;
  this->m31 = other.m31, this->m32 = other.m32, this->m33 = other.m33, this->m34 = other.m34;
  this->m41 = other.m41, this->m42 = other.m42, this->m43 = other.m43, this->m44 = other.m44;
  return *this;
}

Matrix4x4 Matrix4x4::operator+(const Matrix4x4& other) const {
  return {
      this->m11 + other.m11, this->m12 + other.m12, this->m13 + other.m13, this->m14 + other.m14,
      this->m21 + other.m21, this->m22 + other.m22, this->m23 + other.m23, this->m24 + other.m24,
      this->m31 + other.m31, this->m32 + other.m32, this->m33 + other.m33, this->m34 + other.m34,
      this->m41 + other.m41, this->m42 + other.m42, this->m43 + other.m43, this->m44 + other.m44
  };
}

Matrix4x4 Matrix4x4::operator*(float scalar) const {
  return {
      this->m11 * scalar, this->m12 * scalar, this->m13 * scalar, this->m14 * scalar,
      this->m21 * scalar, this->m22 * scalar, this->m23 * scalar, this->m24 * scalar,
      this->m31 * scalar, this->m32 * scalar, this->m33 * scalar, this->m34 * scalar,
      this->m41 * scalar, this->m42 * scalar, this->m43 * scalar, this->m44 * scalar
  };
}

// Row-vector multiply: result = v * M (translation in m41..m43)
Vector4 Matrix4x4::operator*(const Vector4& other) const {
  return {
    other.x * this->m11 + other.y * this->m21 + other.z * this->m31 + other.w * this->m41,
    other.x * this->m12 + other.y * this->m22 + other.z * this->m32 + other.w * this->m42,
    other.x * this->m13 + other.y * this->m23 + other.z * this->m33 + other.w * this->m43,
    other.x * this->m14 + other.y * this->m24 + other.z * this->m34 + other.w * this->m44
  };
}

Vector3 Matrix4x4::operator*(const Vector3& other) const {
  return {
    other.x * this->m11 + other.y * this->m21 + other.z * this->m31 + this->m41,
    other.x * this->m12 + other.y * this->m22 + other.z * this->m32 + this->m42,
    other.x * this->m13 + other.y * this->m23 + other.z * this->m33 + this->m43,
  };
}

Matrix4x4 Matrix4x4::operator-(const Matrix4x4& other) const {
  return {
      this->m11 - other.m11, this->m12 - other.m12, this->m13 - other.m13, this->m14 - other.m14,
      this->m21 - other.m21, this->m22 - other.m22, this->m23 - other.m23, this->m24 - other.m24,
      this->m31 - other.m31, this->m32 - other.m32, this->m33 - other.m33, this->m34 - other.m34,
      this->m41 - other.m41, this->m42 - other.m42, this->m43 - other.m43, this->m44 - other.m44
  };
}

Matrix4x4 Matrix4x4::operator/(const Matrix4x4& other) const {
  return {
      this->m11 / other.m11, this->m12 / other.m12, this->m13 / other.m13, this->m14 / other.m14,
      this->m21 / other.m21, this->m22 / other.m22, this->m23 / other.m23, this->m24 / other.m24,
      this->m31 / other.m31, this->m32 / other.m32, this->m33 / other.m33, this->m34 / other.m34,
      this->m41 / other.m41, this->m42 / other.m42, this->m43 / other.m43, this->m44 / other.m44
  };
}

bool Matrix4x4::operator==(const Matrix4x4& other) const {
  return (
      this->m11 == other.m11 && this->m12 == other.m12 && this->m13 == other.m13 && this->m14 == other.m14 &&
      this->m21 == other.m21 && this->m22 == other.m22 && this->m23 == other.m23 && this->m24 == other.m24 &&
      this->m31 == other.m31 && this->m32 == other.m32 && this->m33 == other.m33 && this->m34 == other.m34 &&
      this->m41 == other.m41 && this->m42 == other.m42 && this->m43 == other.m43 && this->m44 == other.m44
  );
}

/*
  행렬 곱
  +-----------------+   +-----------------+   +-----------------+  
  | m11 m12 m13 m14 |   | n11 n12 n13 n14 |   | r11 r12 r13 r14 |  
  | m21 m22 m23 m24 | X | n21 n22 n23 n24 | = | r21 r22 r23 r24 |  
  | m31 m32 m33 m34 |   | n31 n32 n33 n34 |   | r31 r32 r33 r34 |  
  | m41 m42 m43 m44 |   | n41 n42 n43 n44 |   | r41 r42 r43 r44 |  
  +-----------------+   +-----------------+   +-----------------+  
  ...
*/
Matrix4x4 Matrix4x4::operator*(const Matrix4x4& other) const {
  return {
    // 첫 번째 행
    this->m11 * other.m11 + this->m12 * other.m21 + this->m13 * other.m31 + this->m14 * other.m41,
    this->m11 * other.m12 + this->m12 * other.m22 + this->m13 * other.m32 + this->m14 * other.m42,
    this->m11 * other.m13 + this->m12 * other.m23 + this->m13 * other.m33 + this->m14 * other.m43,
    this->m11 * other.m14 + this->m12 * other.m24 + this->m13 * other.m34 + this->m14 * other.m44,

    // 두 번째 행
    this->m21 * other.m11 + this->m22 * other.m21 + this->m23 * other.m31 + this->m24 * other.m41,
    this->m21 * other.m12 + this->m22 * other.m22 + this->m23 * other.m32 + this->m24 * other.m42,
    this->m21 * other.m13 + this->m22 * other.m23 + this->m23 * other.m33 + this->m24 * other.m43,
    this->m21 * other.m14 + this->m22 * other.m24 + this->m23 * other.m34 + this->m24 * other.m44,

    // 세 번째 행
    this->m31 * other.m11 + this->m32 * other.m21 + this->m33 * other.m31 + this->m34 * other.m41,
    this->m31 * other.m12 + this->m32 * other.m22 + this->m33 * other.m32 + this->m34 * other.m42,
    this->m31 * other.m13 + this->m32 * other.m23 + this->m33 * other.m33 + this->m34 * other.m43,
    this->m31 * other.m14 + this->m32 * other.m24 + this->m33 * other.m34 + this->m34 * other.m44,

    // 네 번째 행
    this->m41 * other.m11 + this->m42 * other.m21 + this->m43 * other.m31 + this->m44 * other.m41,
    this->m41 * other.m12 + this->m42 * other.m22 + this->m43 * other.m32 + this->m44 * other.m42,
    this->m41 * other.m13 + this->m42 * other.m23 + this->m43 * other.m33 + this->m44 * other.m43,
    this->m41 * other.m14 + this->m42 * other.m24 + this->m43 * other.m34 + this->m44 * other.m44
  };
}

void Matrix4x4::Translate(float x, float y, float z) {
  this->m41 += x;
  this->m42 += y;
  this->m43 += z;
}

Vector3 Matrix4x4::Transform(const Vector3& v) {
  return {
    v.x * this->m11 + v.y * this->m21 + v.z * this->m31 + this->m41,
    v.x * this->m12 + v.y * this->m22 + v.z * this->m32 + this->m42,
    v.x * this->m13 + v.y * this->m23 + v.z * this->m33 + this->m43
  };
}

Vector4 Matrix4x4::Transform4(const Vector3& v) {
  return {
    v.x * this->m11 + v.y * this->m21 + v.z * this->m31 + this->m41,
    v.x * this->m12 + v.y * this->m22 + v.z * this->m32 + this->m42,
    v.x * this->m13 + v.y * this->m23 + v.z * this->m33 + this->m43,
    v.x * this->m14 + v.y * this->m24 + v.z * this->m34 + this->m44
  };
}

void Matrix4x4::Rotate(float x, float y, float z) {
  Matrix4x4 rotateX = Matrix4x4::identity;
  Matrix4x4 rotateY = Matrix4x4::identity;
  Matrix4x4 rotateZ = Matrix4x4::identity;

  rotateX.RotateX(x);
  rotateY.RotateY(y);
  rotateZ.RotateZ(z);

  // Row-vector convention: v' = ((v * Rx) * Ry) * Rz
  // so combined matrix is Rx * Ry * Rz.
  *this = rotateX * rotateY * rotateZ;
}

void Matrix4x4::RotateX(float deg) {
  static const float DEG2RAD = acos(-1.0f) / 180;
  float cs = (float)cos(deg * DEG2RAD);
  float ss = (float)sin(deg * DEG2RAD);

  this->m22 = cs;
  this->m23 = ss;
  this->m32 = -ss;
  this->m33 = cs;
}

void Matrix4x4::RotateY(float deg) {
  static const float DEG2RAD = acos(-1.0f) / 180;
  float cs = (float)cos(deg * DEG2RAD);
  float ss = (float)sin(deg * DEG2RAD);

  this->m11 = cs;
  this->m13 = -ss;
  this->m31 = ss;
  this->m33 = cs;
}

void Matrix4x4::RotateZ(float deg) {
  static const float DEG2RAD = acos(-1.0f) / 180;
  float cs = (float)cos(deg * DEG2RAD);
  float ss = (float)sin(deg * DEG2RAD);

  this->m11 = cs;
  this->m12 = ss;
  this->m21 = -ss;
  this->m22 = cs;
}

void Matrix4x4::Print() const {
  static const char* log = R"(
    [ %4.2f %4.2f %4.2f %4.2f ]
    [ %4.2f %4.2f %4.2f %4.2f ]
    [ %4.2f %4.2f %4.2f %4.2f ]
    [ %4.2f %4.2f %4.2f %4.2f ]

)";
  printf(log, m11, m12, m13, m14, 
              m21, m22, m23, m24, 
              m31, m32, m33, m34, 
              m41, m42, m43, m44);
}

namespace math {

void SetupCameraMatrix(Matrix4x4& model, const Vector3& eye, const Vector3& at, const Vector3& up) {
  // https://arienbv.org/blog/2017/07/30/breakdown-of-the-lookAt-function-in-OpenGL/
  // eye는 카메라의 위치, at은 카메라가 바라보고 있는 방향, up은 카메라 기준 위 방향
  
  // Left-handed 좌표계: Forward는 +Z 방향.

  //Vector3 Vector3::Normalize() const {
  //  const float mag = (float)std::sqrt(this->x * this->x + this->y * this->y +
  //    this->z * this->z);
  //  return (*this) / mag;
  //}

  Vector3 f = (at - eye).Normalize();
  Vector3 zaxis = f;

  // 두 개의 다른 벡터 축 계산
  // 다음 두 벡터를 계산하기 위해서는 외적이 효과적
  
  //  1. Up과 Forward를 외적하여 Right(x축) 구함.
  //     Up 벡터는 유닛 벡터가 아닐 수 있으므로 정규화 필요.

  //Vector3 CrossProduct(const Vector3 & v1, const Vector3 & v2) {
  //  return {
  //    v1.y * v2.z - v1.z * v2.y,
  //    v1.z * v2.x - v1.x * v2.z,
  //    v1.x * v2.y - v1.y * v2.z
  //  };
  //}
  Vector3 xaxis = (math::CrossProduct(up, zaxis)).Normalize();
  
  //  2. Forward와 Right로 Up(y축) 재계산
  Vector3 yaxis = math::CrossProduct(zaxis, xaxis);

  /*
  +-----------------+
  | m11 m12 m13 m14 |
  | m21 m22 m23 m24 |
  | m31 m32 m33 m34 |
  | m41 m42 m43 m44 |
  +-----------------+
  */
  // 위 세 개의 축을 구한 뒤 주어진 모델 좌표계를
  // 카메라 좌표계로 이동하려면 어파인 변환을 거쳐야 함
  // 어파인 변환은 이동과 회전으로 나뉘며 이를 적용할 때
  // 이들을 분리하여 변환 행렬의 역행렬을 구하기 쉽도록 함
  // X, Y, Z, 순으로 채우고 // W 쪽 채우기

  //float DotProduct(const Vector3 & v1, const Vector3 & v2) {
  //  return v1.x * v2.x + v1.y * v2.y + v1.z + v2.z;
  //}

  model.m11 = xaxis.x;  model.m12 = yaxis.x;  model.m13 = zaxis.x;  model.m14 = 0.0f;
  model.m21 = xaxis.y;  model.m22 = yaxis.y;  model.m23 = zaxis.y;  model.m24 = 0.0f;
  model.m31 = xaxis.z;  model.m32 = yaxis.z;  model.m33 = zaxis.z;  model.m34 = 0.0f;
  model.m41 = -math::DotProduct(xaxis, eye);
  model.m42 = -math::DotProduct(yaxis, eye);
  model.m43 = -math::DotProduct(zaxis, eye);
  model.m44 = 1.0f;
}

void SetupPerspectiveProjectionMatrix(Matrix4x4& out, float fovY, float aspect, float near, float far) {
  // 원근 투영 (Left-handed)
  // https://www.songho.ca/opengl/gl_projectionmatrix.html

  const float DEG2RAD = acos(-1.0f) / 180;

  float tanfov = (float)tan(fovY * 0.5f * DEG2RAD);
  out.m11 = (1.0f / tanfov) / aspect;
  out.m22 = 1.0f / tanfov;
  out.m33 = far / (far - near);
  out.m34 = 1.0f;
  out.m43 = -(near * far) / (far - near);
  out.m44 = 0.0f;
}

void SetupViewportMatrix(Matrix4x4& out, float x, float y, float w, float h, float near, float far) {
  /*
  * https://www.songho.ca/opengl/gl_viewport.html
  +-----------------+ +----------------------+
  | m11  0   0  m14 | | w/2  0    0    x+w/2 |
  |  0  m22  0  m24 | |  0  h/2   0    y+h/2 |
  |  0   0  m33 m34 | |  0   0  f-n/2  f+n/2 |
  |  0   0  m43 m44 | |  0   0    0      1   |
  +-----------------+ +----------------------+
  */
  // https://www.songho.ca/opengl/gl_transform.html
  // https://www.songho.ca/opengl/gl_viewport.html

  out.m11 = w * 0.5f;
  // 화면 좌표계는 Y가 아래로 증가하므로 NDC Y를 뒤집는다.
  out.m22 = -h * 0.5f;
  out.m33 = 1.0f;
  out.m41 = x + w * 0.5f;
  out.m42 = y + h * 0.5f;
  out.m43 = 0.0f;
  out.m44 = 1.0f;
}

uint32_t LerpColor(uint32_t from, uint32_t to, float t) {
  return (uint32_t)(from + t * (to - from));
}

float EdgeFunction(const Vector2& a, const Vector2& b, float x, float y) {
  return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
}

} // namespace math
