#pragma once

#include "Mesh.hpp"
#include "Math.hpp"
#include "Shader.hpp"

struct Renderable
{
    Mesh *mesh{nullptr}; // Reference
    Matrix4x4 modelMatrix;
    Shader *shader{nullptr}; // Reference

    Renderable() : modelMatrix(Matrix4x4::identity) {}
};