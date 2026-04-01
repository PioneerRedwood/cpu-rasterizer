#pragma once

#include "Math.hpp"

struct TriangleSetup
{
    Vector3 v0;
    Vector3 v1;
    Vector3 v2;

    float area = 0.0f;
    float invArea = 0.0f;

    int minX = 0;
    int maxX = 0;
    int minY = 0;
    int maxY = 0;
};