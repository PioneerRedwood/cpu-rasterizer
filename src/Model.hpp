#pragma once

#include "Math.hpp"
#include "Mesh.hpp"

struct Model {
  Mesh* mesh {nullptr};
  Matrix4x4 modelMatrix;

  Model() : modelMatrix(Matrix4x4::identity) {}
};
