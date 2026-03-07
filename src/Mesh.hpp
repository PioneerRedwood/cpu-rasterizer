#pragma once

#include <cstdint>
#include <vector>

#include "Math.hpp"
#include "TGA.hpp"

struct SimpleMesh {
  std::vector<Vector3> verts;
  std::vector<uint32_t> indices;
  std::vector<Vector2> uvs;
  TGA* tga;
};
