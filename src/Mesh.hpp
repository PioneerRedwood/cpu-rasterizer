#pragma once

#include "Math.hpp"
#include "TGA.hpp"

#include <vector>

struct Mesh
{
    std::vector<Vector3> verts;
    std::vector<uint32_t> indices;
    std::vector<Vector2> uvs;
    std::vector<Vector3> normals;
    TGA *tga{nullptr};
    bool hasUVs{false};
    bool hasNormals{false};
};
