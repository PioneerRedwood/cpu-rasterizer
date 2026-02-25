#pragma once

#include "Math.hpp"

struct WorldCamera {
    float aspect { 0 };
    float fov { 0 };
    
    Vector3 eye;
    Vector3 at;
    Vector3 up;
};
