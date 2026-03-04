//------------------------------------------------------------------------------
// File: RGBA.hpp
// Author: Chris Redwood
// Created: 2024-10-21
// License: MIT License
//------------------------------------------------------------------------------

#pragma once

#include <cstdint>

struct RGBA {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

struct RGBAf {
  float r, g, b, a;
};
