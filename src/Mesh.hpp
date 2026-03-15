#pragma once

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Math.hpp"
#include "TGA.hpp"

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
