#pragma once

#include "Math.hpp"

struct WorldCamera
{
    float aspect{0.0f};
    float fov{0.0f};

    Vector3 eye{2.2f, 1.2f, -6.5f};
    Vector3 at{0.0f, 0.0f, 0.0f};
    Vector3 up{0.0f, 1.0f, 0.0f};
};
