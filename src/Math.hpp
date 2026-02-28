//------------------------------------------------------------------------------
// File: Math.hpp
// Author: Chris Redwood
// Created: 2024-10-30
// License: MIT License
//------------------------------------------------------------------------------

#pragma once

#include <vector>
#include <cstdlib>
#include <cmath>
#include <string>

struct Vector2 {
  float x;
  float y;

  Vector2();
  Vector2(float x, float y);

  Vector2 operator+(const Vector2& other) const;
  Vector2 operator*(float scalar) const;
  Vector2& operator=(const Vector2& other);
  Vector2 operator-(const Vector2& other) const;
  bool operator==(const Vector2& other) const;

  /// @brief 벡터의 크기 계산
  float magnitude() const;

  /// @brief 벡터 노멀라이즈 (정규화)
  Vector2 normalized() const;
};

struct Vector3 {
  float x;
  float y;
  float z;

  Vector3();
  Vector3(float x, float y, float z);

  Vector3(const Vector3& other);
  Vector3& operator=(const Vector3& other);

  Vector3 operator+(const Vector3& other) const;
  Vector3 operator*(float scalar) const;
  Vector3 operator-(const Vector3& other) const;
  Vector3 operator/(const Vector3& other) const;
  Vector3 operator/(float value) const;
  bool operator==(const Vector3& other) const;

  /// @brief 각 값을 정규화
  /// @return 정규화된 벡터 반환
  Vector3 normalize() const;

  std::string toString() const;
};

namespace math {

/// @brief 두 벡터의 뺄셈을 반환
/// @param v1 첫번째 벡터
/// @param v2 두번째 벡터
/// @return 두 벡터의 뺄셈
Vector3 subtract(const Vector3& v1, const Vector3& v2);

/// @brief 두 벡터의 내적을 반환. 스칼라 값 출력. v1 · v2 = |v1| |v2| cosθ
/// https://en.wikipedia.org/wiki/Dot_product 참고.
/// 두 벡터가 모두 영 벡터가 아닐때 내적이 0이면 두 벡터의 사이각은 반드시 90도.
/// 영 벡터가 아닌 두 벡터의 길이는 항상 양이므로 내적이 0보다 크면 사이각이 
/// 90도 보다 작고, 반대로 0보다 작으면 90도 보다 크다. 
/// @param v1 첫번째 벡터
/// @param v2 두번째 벡터
/// @return 두 벡터의 내적
float dotProduct(const Vector3& v1, const Vector3& v2);

/// @brief 두 벡터의 외적을 반환. 벡터 출력. v1 X v2 = |v1| |v2| sinθ
/// https://en.wikipedia.org/wiki/Cross_product 참고
/// @param v1 첫번째 벡터
/// @param v2 두번째 벡터
/// @return 두 벡터의 외적
Vector3 crossProduct(const Vector3& v1, const Vector3& v2);

}

struct Vector4 {
  float x;
  float y;
  float z;
  float w;

  Vector4();
  Vector4(float, float, float, float);
  void perspectiveDivide();
};

/*
  Row-vector convention (v * M):
  [ x y z w ] * M =
  [ x*m11 + y*m21 + z*m31 + w*m41,
    x*m12 + y*m22 + z*m32 + w*m42,
    x*m13 + y*m23 + z*m33 + w*m43,
    x*m14 + y*m24 + z*m34 + w*m44 ]
*/
struct Matrix4x4 {
  float m11, m12, m13, m14;
  float m21, m22, m23, m24;
  float m31, m32, m33, m34;
  float m41, m42, m43, m44;

  Matrix4x4();
  Matrix4x4(float, float, float, float,
            float, float, float, float,
            float, float, float, float,
            float, float, float, float);

  // Note: Engine uses left-handed coordinates; columns store basis vectors and last row is translation.
  Matrix4x4(Vector4 x, Vector4 y, Vector4 z, Vector4 w);
  Matrix4x4(const Vector4&, const Vector4&, const Vector4&, const Vector4&);

  Matrix4x4(const Matrix4x4& other);
  Matrix4x4& operator=(const Matrix4x4& other);

  Matrix4x4 operator+(const Matrix4x4& other) const;
  Matrix4x4 operator*(float scalar) const;
  Matrix4x4 operator-(const Matrix4x4& other) const;
  Matrix4x4 operator/(const Matrix4x4& other) const;
  bool operator==(const Matrix4x4& other) const;
  
  // multiply
  Matrix4x4 operator*(const Matrix4x4& other) const;
  Vector4 operator*(const Vector4& other) const;
  Vector3 operator*(const Vector3& other) const;

  static Matrix4x4 identity;

  /// @brief 주어진 좌표로 이동하도록 하는 행렬 수정
  void translate(float x, float y, float z);
  
  /// @brief 주어진 좌표로 변환한 벡터를 구함
  /// @param v 3차원 좌표
  /// @return 3차원 벡터
  Vector3 transform(const Vector3& v);
  
  /// @brief 주어진 좌표로 변환한 벡터를 구함
  /// @param v 4차원 좌표
  /// @return 4차원 벡터
  Vector4 transform4(const Vector3& v);

  /// @brief 주어진 각도로 행렬 회전
  void rotate(float x, float y, float z);

  void rotateX(float deg);

  void rotateY(float deg);

  void rotateZ(float deg);

  void print() const;
};

namespace math {

/**
 * @brief 현재 카메라가 바라보고 있는 방향의 벡터를 반환
 */
void setupCameraMatrix(Matrix4x4& out, const Vector3& eye, const Vector3& at, const Vector3& up);

/**
 * @brief 원근 투영 프러스텀 매트릭스 반환
 */
void setupPerspectiveProjectionMatrix(Matrix4x4& out, float fovY, float aspect, float near, float far);

/**
 * @brief 뷰포트 행렬 구성
 * near, far는 기본값 각각 0, 1 사용
 */
void setupViewportMatrix(Matrix4x4& out, float x, float y, float w, float h, float near, float far);

uint32_t lerpColor(uint32_t from, uint32_t to, float t);

/**
 * 두 벡터로 이루어진 면과 주어진 정점 사이를 수학적으로 계산
 * 결과값이 0인 경우: 그 면에 정점이 위치
 * 결과값이 0보다 큰 경우: 그 면보다 앞에 위치
 * 결과값이 0보다 작은 경우: 그 면보다 뒤에 위치
 * 
 * "한 점이 삼각형의 각 변 기준으로 어느 쪽에 있는지" 판단하는 것도 가능
 * x, y를 각 값으로 가지고 있는 정점을 P라고 가정 (결과값은 edge)
 * edge > 0  : 점P는 선분 a->b 왼쪽에 위치
 * edge < 0  : 점P는 선분 a->b 오른쪽에 위치
 * edge == 0 : 점P는 선분에 위치
 */
float edgeFunction(const Vector2& a, const Vector2& b, float x, float y);

} // namespace math
