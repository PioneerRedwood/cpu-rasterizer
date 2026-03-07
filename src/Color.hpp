//------------------------------------------------------------------------------
// File: Color.hpp
// Author: Chris Redwood
// Created: 2024-10-21
// License: MIT License
//------------------------------------------------------------------------------

#pragma once

#include <cstdint>

struct Color {
  union {
    struct {
      uint8_t r, g, b, a;
    };
    uint32_t value;
  };

  constexpr Color() : value(0) {}
  constexpr explicit Color(uint32_t v) : value(v) {}
  constexpr operator uint32_t() const {
    return value;
  }  // Allow static_cast<uint32_t>(color)
};

struct Colorf {
  float r, g, b, a;
};
