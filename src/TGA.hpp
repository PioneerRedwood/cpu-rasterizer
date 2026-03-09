//------------------------------------------------------------------------------
// File: TGA.hpp
// Author: Chris Redwood
// Created: 2024-10-21
// License: MIT License
//------------------------------------------------------------------------------

#pragma once

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Color.hpp"

/**
 * TGA (Truevision Graphics Adapter), TARGA (Truevision Advanced Raster Graphics
 * Adapter) https://en.wikipedia.org/wiki/Truevision_TGA
 */

#pragma pack(push, 1)
struct TGAHeader {
  // Image ID Length (field 1)
  uint8_t id_length;

  // Color Map Type (field 2)
  uint8_t color_map_type;

  // Image Type (field 3)
  uint8_t image_type;

  // Color Map Specification (field 4)
  uint16_t first_entry_index;
  uint16_t color_map_length;
  uint8_t color_map_entry_size;

  // Image Specification (field 5)
  uint16_t x_origin;
  uint16_t y_origin;
  uint16_t width;
  uint16_t height;
  uint8_t pixel_depth;  // bits per pixel
  uint8_t image_descriptor;

  // Image and Color Map Data
  // Image ID
  // Color map data
};

// Not used
struct TGAFooter {
  uint32_t extension_offset;
  uint32_t developer_area_offset;
  uint16_t signature;
  uint8_t last_period;
  uint8_t nul;
};
#pragma pack(pop)

class TGA {
 public:
  TGA() = default;

  ~TGA() {
    if (m_PixelData != nullptr) {
      delete[] m_PixelData;
    }

    if (m_Texture != nullptr) {
      SDL_DestroyTexture(m_Texture);
    }
  }

  const TGAHeader* Header() const { return (TGAHeader const*)&m_Header; }

  const Color* PixelData() const { return m_PixelData; }

  SDL_Texture const* SDLTexture() const { return m_Texture; }

  bool ReadFromFile(const char* filepath, uint32_t supportedPixelFormat) {
    FILE* fp = fopen(filepath, "rb");
    if (fp == nullptr) {
      return false;
    }

    size_t read = fread(&m_Header, sizeof(TGAHeader), 1, fp);
    if (read != 1) {
      fclose(fp);
      return false;
    }
      
    if (m_Header.image_type != 2) {
      fclose(fp);
      return false;
    }

    const int bytesPerPixel = static_cast<int>(m_Header.pixel_depth) / 8;
    if (bytesPerPixel != 3 && bytesPerPixel != 4) {
      fclose(fp);
      return false;
    }

    const size_t pixelCount =
        static_cast<size_t>(m_Header.width) * static_cast<size_t>(m_Header.height);
    if (pixelCount == 0) {
      fclose(fp);
      return false;
    }

    if (m_PixelData != nullptr) {
      delete[] m_PixelData;
      m_PixelData = nullptr;
    }
    m_PixelData = new Color[pixelCount];

    if (m_Header.id_length > 0) {
      if (fseek(fp, m_Header.id_length, SEEK_CUR) != 0) {
        fclose(fp);
        return false;
      }
    }

    const size_t imageSize = pixelCount * static_cast<size_t>(bytesPerPixel);
    std::vector<uint8_t> rawPixels(imageSize);
    read = fread(rawPixels.data(), imageSize, 1, fp);
    if (read != 1) {
      fclose(fp);
      return false;
    }

    const bool topLeftOrigin = (m_Header.image_descriptor & 0x20) != 0;
    const bool targetHasAlpha = SDL_ISPIXELFORMAT_ALPHA(supportedPixelFormat);

    for (uint16_t y = 0; y < m_Header.height; ++y) {
      for (uint16_t x = 0; x < m_Header.width; ++x) {
        const size_t srcIndex =
            (static_cast<size_t>(y) * m_Header.width + x) * bytesPerPixel;

        const uint8_t sourceB = rawPixels[srcIndex + 0];
        const uint8_t sourceG = rawPixels[srcIndex + 1];
        const uint8_t sourceR = rawPixels[srcIndex + 2];
        const uint8_t sourceA = (bytesPerPixel == 4 && targetHasAlpha)
                                    ? rawPixels[srcIndex + 3]
                                    : 255;

        const uint16_t dstY = topLeftOrigin ? y : static_cast<uint16_t>(m_Header.height - 1 - y);
        const size_t dstIndex = static_cast<size_t>(dstY) * m_Header.width + x;
        Color& dstColor = m_PixelData[dstIndex];

        // TGA source is BGR(A); convert to logical RGBA for the sampler.
        dstColor.r = sourceR;
        dstColor.g = sourceG;
        dstColor.b = sourceB;
        dstColor.a = sourceA;
      }
    }

    fclose(fp);
    return true;
  }

  bool CreateTexture(SDL_Renderer* renderer, uint32_t format) {
    if (renderer == nullptr) {
      return false;
    }

    if (m_Texture != nullptr) {
      SDL_DestroyTexture(m_Texture);
      m_Texture = nullptr;
    }

    m_Texture = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STATIC,
                                  m_Header.width, m_Header.height);
    if (m_Texture == nullptr) {
      return false;
    }
    const int pitch = m_Header.width * (int)sizeof(Color);
    if (SDL_UpdateTexture(m_Texture, nullptr, m_PixelData, pitch) != 0) {
      SDL_assert(false);
      return false;
    }

    // If only rendering textures by SDL_Texture APIs then donot have to delete
    // the pixel memory delete[] m_pixel_data; m_pixel_data = nullptr;

    return true;
  }

 private:
  TGAHeader m_Header{};
  Color* m_PixelData = nullptr;
  SDL_Texture* m_Texture = nullptr;
};
