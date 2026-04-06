#pragma once

#include "Mesh.hpp"
#include "Math.hpp"
#include "Shader.hpp"
#include "Material.hpp"

struct Renderable
{
    Mesh *mesh{nullptr}; // Reference
    Matrix4x4 modelMatrix;
    Shader *shader{nullptr}; // Reference
    Material material;

    Renderable() : modelMatrix(Matrix4x4::identity) {}
};