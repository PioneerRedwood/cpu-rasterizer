//------------------------------------------------------------------------------
// File: TGA.hpp
// Author: Chris Redwood
// Created: 2024-10-21
// License: MIT License
//------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <cstdio>

#include <SDL.h>
#include "RGBA.hpp"

/**
 * TGA (Truevision Graphics Adapter), TARGA (Truevision Advanced Raster Graphics Adapter)
 * https://en.wikipedia.org/wiki/Truevision_TGA
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
        if (m_pixelData != nullptr)
        {
            delete[] m_pixelData;
        }

        if (m_texture != nullptr)
        {
            SDL_DestroyTexture(m_texture);
        }
    }

    const TGAHeader *header() const
    {
        return (TGAHeader const *)&m_header;
    }

    const RGBA *pixelData() const
    {
        return m_pixelData;
    }

    SDL_Texture const *sdlTexture() const
    {
        return m_texture;
    }

    bool readFromFile(const char *filepath)
    {
        FILE *fp = fopen(filepath, "rb");
        if (fp == nullptr)
        {
            return false;
        }

        const size_t read = fread(&m_header, sizeof(TGAHeader), 1, fp);
        if (read != 1)
        {
            fclose(fp);
            return false;
        }

        if (m_header.color_map_type != 0)
        {
            fclose(fp);
            return false;
        }

        const int bytesPerPixel = m_header.pixel_depth / 8;
        if (bytesPerPixel != 3 && bytesPerPixel != 4)
        {
            fclose(fp);
            return false;
        }

        // 2: uncompressed true-color, 10: RLE true-color.
        if (m_header.image_type != 2 && m_header.image_type != 10)
        {
            fclose(fp);
            return false;
        }

        if (m_header.id_length > 0)
        {
            if (fseek(fp, m_header.id_length, SEEK_CUR) != 0)
            {
                fclose(fp);
                return false;
            }
        }

        const uint32_t width = m_header.width;
        const uint32_t height = m_header.height;
        const uint32_t pixelCount = width * height;
        if (pixelCount == 0)
        {
            fclose(fp);
            return false;
        }

        if (m_pixelData != nullptr)
        {
            delete[] m_pixelData;
            m_pixelData = nullptr;
        }
        m_pixelData = new RGBA[pixelCount];

        auto readPixel = [fp, bytesPerPixel](RGBA &out) -> bool
        {
            uint8_t px[4] = {0, 0, 0, 255};
            if (fread(px, bytesPerPixel, 1, fp) != 1)
            {
                return false;
            }

            // Keep TGA byte order in memory (B,G,R,A) for SDL_PIXELFORMAT_BGRA32 upload.
            out.r = px[0];
            out.g = px[1];
            out.b = px[2];
            out.a = (bytesPerPixel == 4) ? px[3] : 255;
            return true;
        };

        const bool topLeftOrigin = (m_header.image_descriptor & 0x20) != 0;
        auto writePixel = [this, width, height, topLeftOrigin](uint32_t srcIndex, const RGBA &value)
        {
            uint32_t dstIndex = srcIndex;
            if (!topLeftOrigin)
            {
                const uint32_t x = srcIndex % width;
                const uint32_t y = srcIndex / width;
                dstIndex = x + (height - 1 - y) * width;
            }
            m_pixelData[dstIndex] = value;
        };

        if (m_header.image_type == 2)
        {
            for (uint32_t i = 0; i < pixelCount; ++i)
            {
                RGBA pixel{};
                if (!readPixel(pixel))
                {
                    fclose(fp);
                    return false;
                }
                writePixel(i, pixel);
            }
        }
        else
        {
            uint32_t i = 0;
            while (i < pixelCount)
            {
                uint8_t packetHeader = 0;
                if (fread(&packetHeader, 1, 1, fp) != 1)
                {
                    fclose(fp);
                    return false;
                }

                const uint32_t count = (packetHeader & 0x7F) + 1;
                if (i + count > pixelCount)
                {
                    fclose(fp);
                    return false;
                }

                if ((packetHeader & 0x80) != 0)
                {
                    RGBA pixel{};
                    if (!readPixel(pixel))
                    {
                        fclose(fp);
                        return false;
                    }

                    for (uint32_t k = 0; k < count; ++k)
                    {
                        writePixel(i + k, pixel);
                    }
                }
                else
                {
                    for (uint32_t k = 0; k < count; ++k)
                    {
                        RGBA pixel{};
                        if (!readPixel(pixel))
                        {
                            fclose(fp);
                            return false;
                        }
                        writePixel(i + k, pixel);
                    }
                }

                i += count;
            }
        }

        fclose(fp);
        return true;
    }

    bool createTexture(SDL_Renderer *renderer)
    {
        m_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA32,
                                      SDL_TEXTUREACCESS_STATIC, m_header.width, m_header.height);
        const int pitch = m_header.width * (int)sizeof(RGBA);
        if (SDL_UpdateTexture(m_texture, nullptr, m_pixelData, pitch) != 0)
        {
            SDL_assert(false);
            return false;
        }

        // If only rendering textures by SDL_Texture APIs then donot have to delete the pixel memory
        // delete[] m_pixel_data;
        // m_pixel_data = nullptr;

        return true;
    }

private:
    TGAHeader m_header{};
    RGBA *m_pixelData = nullptr;
    SDL_Texture *m_texture = nullptr;
};
