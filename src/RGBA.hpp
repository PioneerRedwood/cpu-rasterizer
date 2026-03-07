//------------------------------------------------------------------------------
// File: RGBA.hpp
// Author: Chris Redwood
// Created: 2024-10-21
// License: MIT License
//------------------------------------------------------------------------------

#pragma once

#include <cstdint>

struct RGBA {
  union {
    struct {
      uint8_t r, g, b, a;
    };
    uint32_t
        value;  // On little-endian environment, allows memory mapping as ABGR
  };

  constexpr RGBA() : value(0) {}
  constexpr explicit RGBA(uint32_t v) : value(v) {}
  constexpr operator uint32_t() const {
    return value;
  }  // Allow static_cast<uint32_t>(color)
};

struct RGBAf {
  float r, g, b, a;
};
