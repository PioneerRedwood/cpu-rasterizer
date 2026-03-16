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
struct TGAHeader
{
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
  uint8_t pixel_depth; // bits per pixel
  uint8_t image_descriptor;

  // Image and Color Map Data
  // Image ID
  // Color map data
};

// Not used
struct TGAFooter
{
  uint32_t extension_offset;
  uint32_t developer_area_offset;
  uint16_t signature;
  uint8_t last_period;
  uint8_t nul;
};
#pragma pack(pop)

class TGA
{
public:
  TGA() = default;

  ~TGA()
  {
    if (m_PixelData != nullptr)
    {
      delete[] m_PixelData;
    }

    if (m_Texture != nullptr)
    {
      SDL_DestroyTexture(m_Texture);
    }
  }

  const TGAHeader *Header() const { return (TGAHeader const *)&m_Header; }

  const Color *PixelData() const { return m_PixelData; }

  SDL_Texture const *SDLTexture() const { return m_Texture; }

  bool ReadFromFile(const char *filepath, uint32_t supportedPixelFormat)
  {
    FILE *fp = fopen(filepath, "rb");
    if (fp == nullptr)
    {
      return false;
    }

    size_t read = fread(&m_Header, sizeof(TGAHeader), 1, fp);
    if (read != 1)
    {
      fclose(fp);
      return false;
    }

    if (m_Header.image_type != 2)
    {
      fclose(fp);
      return false;
    }

    const int bytesPerPixel = static_cast<int>(m_Header.pixel_depth) / 8;
    if (bytesPerPixel != 3 && bytesPerPixel != 4)
    {
      fclose(fp);
      return false;
    }

    const size_t pixelCount =
        static_cast<size_t>(m_Header.width) * static_cast<size_t>(m_Header.height);
    if (pixelCount == 0)
    {
      fclose(fp);
      return false;
    }

    if (m_PixelData != nullptr)
    {
      delete[] m_PixelData;
      m_PixelData = nullptr;
    }
    m_PixelData = new Color[pixelCount];

    if (m_Header.id_length > 0)
    {
      if (fseek(fp, m_Header.id_length, SEEK_CUR) != 0)
      {
        fclose(fp);
        return false;
      }
    }

    const size_t imageSize = pixelCount * static_cast<size_t>(bytesPerPixel);
    std::vector<uint8_t> rawPixels(imageSize);
    read = fread(rawPixels.data(), imageSize, 1, fp);
    if (read != 1)
    {
      fclose(fp);
      return false;
    }

    const bool topLeftOrigin = (m_Header.image_descriptor & 0x20) != 0;
    const bool targetHasAlpha = SDL_ISPIXELFORMAT_ALPHA(supportedPixelFormat);

    for (uint16_t y = 0; y < m_Header.height; ++y)
    {
      for (uint16_t x = 0; x < m_Header.width; ++x)
      {
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
        Color &dstColor = m_PixelData[dstIndex];

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

  bool CreateTexture(SDL_Renderer *renderer, uint32_t format)
  {
    if (renderer == nullptr)
    {
      return false;
    }

    if (m_Texture != nullptr)
    {
      SDL_DestroyTexture(m_Texture);
      m_Texture = nullptr;
    }

    m_Texture = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STATIC,
                                  m_Header.width, m_Header.height);
    if (m_Texture == nullptr)
    {
      return false;
    }
    const int pitch = m_Header.width * (int)sizeof(Color);
    if (SDL_UpdateTexture(m_Texture, nullptr, m_PixelData, pitch) != 0)
    {
      SDL_assert(false);
      return false;
    }

    // If only rendering textures by SDL_Texture APIs then donot have to delete
    // the pixel memory delete[] m_pixel_data; m_pixel_data = nullptr;

    return true;
  }

  /**
   * Sample the texture at normalized UV coordinates (0.0 to 1.0) using bilinear
   */
  Color Sample(float u, float v) const
  {
    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));
    v = 1.0f - v;

    const int w = (int)m_Header.width;
    const int h = (int)m_Header.height;
    if (w <= 0 || h <= 0)
    {
      return Color{};
    }

    const Color *pixel = m_PixelData;

    const float x = u * (float)(w - 1);
    const float y = v * (float)(h - 1);
    const int x0 = std::max(0, std::min(w - 1, (int)std::floor(x)));
    const int y0 = std::max(0, std::min(h - 1, (int)std::floor(y)));
    const int x1 = std::min(w - 1, x0 + 1);
    const int y1 = std::min(h - 1, y0 + 1);
    const float fx = x - (float)x0;
    const float fy = y - (float)y0;

    const Color &c00 = pixel[x0 + y0 * w]; // top-left
    const Color &c10 = pixel[x1 + y0 * w]; // top-right
    const Color &c01 = pixel[x0 + y1 * w]; // bottom-left
    const Color &c11 = pixel[x1 + y1 * w]; // bottom-right

    auto blendChannel = [fx, fy](uint8_t cc00, uint8_t cc10, uint8_t cc01,
                                 uint8_t cc11) -> uint8_t
    {
      const float top = cc00 + (cc10 - cc00) * fx;
      const float bottom = cc01 + (cc11 - cc01) * fx;
      float value = top + (bottom - top) * fy;
      value = std::max(0.0f, std::min(255.0f, value));
      return static_cast<uint8_t>(value + 0.5f);
    };

    Color result{};
    result.r = blendChannel(c00.r, c10.r, c01.r, c11.r);
    result.g = blendChannel(c00.g, c10.g, c01.g, c11.g);
    result.b = blendChannel(c00.b, c10.b, c01.b, c11.b);
    result.a = blendChannel(c00.a, c10.a, c01.a, c11.a);
    return result;
  }

  /*
  Color Sample(float u, float v) const
  {
    if (texture == nullptr || texture->Header() == nullptr ||
        texture->PixelData() == nullptr)
    {
      return Color{};
    }

    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));
    v = 1.0f - v;

    const int texWidth = (int)texture->Header()->width;
    const int texHeight = (int)texture->Header()->height;
    if (texWidth <= 0 || texHeight <= 0)
    {
      return Color{};
    }

    const int tx =
        std::min(texWidth - 1, std::max(0, (int)(u * (float)(texWidth - 1))));
    const int ty =
        std::min(texHeight - 1, std::max(0, (int)(v * (float)(texHeight - 1))));
    return texture->PixelData()[tx + ty * texWidth];
  }
  */
private:
  TGAHeader m_Header{};
  Color *m_PixelData = nullptr;
  SDL_Texture *m_Texture = nullptr;
};
