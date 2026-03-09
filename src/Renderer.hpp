#pragma once

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Mesh.hpp"
#include "TextureLoader.hpp"
#include "WorldCamera.hpp"

class Renderer {
 public:
  Renderer(SDL_Window* window, int width, int height)
      : m_Width(width), m_Height(height) {
    m_Framebuffer = new uint32_t[m_Width * m_Height];
    m_Camera = new WorldCamera();
    m_ZBuffer = new float[m_Width * m_Height];
    std::fill(m_ZBuffer, m_ZBuffer + m_Width * m_Height, 1.0f);

    m_Renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (m_Renderer == nullptr) {
      LogF("SDL_CreateRenderer failed: %s", SDL_GetError());
      SDL_assert(false);
      return;
    }

    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(m_Renderer, &info) == 0) {
      LogF("Renderer backend: %s", info.name);
      if (info.num_texture_formats > 0) {
        m_FramebufferFormatEnum = info.texture_formats[0];
      }
      for (uint32_t i = 0; i < info.num_texture_formats; ++i) {
        LogF("Supported[%u]: %s", i,
             SDL_GetPixelFormatName(info.texture_formats[i]));
      }
    }

    if (SDL_ISPIXELFORMAT_FOURCC(m_FramebufferFormatEnum) ||
        SDL_BYTESPERPIXEL(m_FramebufferFormatEnum) !=
            static_cast<int>(sizeof(uint32_t))) {
      LogF("Unsupported primary format (%s). Fallback to ARGB8888.",
           SDL_GetPixelFormatName(m_FramebufferFormatEnum));
      m_FramebufferFormatEnum = SDL_PIXELFORMAT_ARGB8888;
    }

    m_FramebufferFormat = SDL_AllocFormat(m_FramebufferFormatEnum);
    if (m_FramebufferFormat == nullptr) {
      LogF("SDL_AllocFormat failed for %s: %s",
           SDL_GetPixelFormatName(m_FramebufferFormatEnum), SDL_GetError());
      m_FramebufferFormatEnum = SDL_PIXELFORMAT_ARGB8888;
      m_FramebufferFormat = SDL_AllocFormat(m_FramebufferFormatEnum);
    }
    if (m_FramebufferFormat != nullptr) {
      LogF("Selected framebuffer format: %s (bpp=%u)",
           SDL_GetPixelFormatName(m_FramebufferFormatEnum),
           m_FramebufferFormat->BitsPerPixel);
    }

    m_MainTexture =
        SDL_CreateTexture(m_Renderer, m_FramebufferFormatEnum,
                          SDL_TEXTUREACCESS_STREAMING, m_Width, m_Height);
    if (m_MainTexture == nullptr) {
      LogF("SDL_CreateTexture failed for %s: %s",
           SDL_GetPixelFormatName(m_FramebufferFormatEnum), SDL_GetError());
      SDL_assert(false);
      return;
    }

    m_Camera->aspect = (float)m_Width / m_Height;
    m_Camera->fov = 45.0f;

    SetupMatrices();

    BuildTriangle();
    BuildCube();

    m_TextureLoader = new TextureLoader("resources");
    //        m_TgaTexture = m_TextureLoader->LoadTGATextureWithName(m_Renderer,
    //        "keep-carm.tga");
    m_TgaTexture = m_TextureLoader->LoadTGATextureWithName(
        m_Renderer, "numbered_checker.tga", m_FramebufferFormatEnum);
    if (m_TgaTexture == nullptr) {
      LogF("Failed to load texture");
    }

    BuildSimpleCubeMesh();
		BuildBunnyMesh();
  }

  ~Renderer() {
    delete[] m_Framebuffer;
    delete[] m_ZBuffer;
    delete m_Camera;
    delete m_TextureLoader;
    if (m_FramebufferFormat != nullptr) {
      SDL_FreeFormat(m_FramebufferFormat);
      m_FramebufferFormat = nullptr;
    }
    SDL_DestroyTexture(m_MainTexture);
    SDL_DestroyRenderer(m_Renderer);
  }

  void Render(double delta) {
    memset((char*)m_Framebuffer, 0, sizeof(uint32_t) * m_Width * m_Height);
    std::fill(m_ZBuffer, m_ZBuffer + m_Width * m_Height, 1.0f);

    // RenderTriangle();
    // RenderCubeLines();
    // RenderCubeMesh(delta);
    RenderBunnyMesh();

    SDL_UpdateTexture(m_MainTexture, nullptr, m_Framebuffer,
                      m_Width * static_cast<int>(sizeof(uint32_t)));
    SDL_RenderCopy(m_Renderer, m_MainTexture, nullptr, nullptr);
    SDL_RenderPresent(m_Renderer);
    SDL_Delay(32);
  }

 private:
  uint32_t PackColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) const {
    if (m_FramebufferFormat == nullptr) {
      return (static_cast<uint32_t>(a) << 24) |
             (static_cast<uint32_t>(r) << 16) |
             (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    }
    return SDL_MapRGBA(m_FramebufferFormat, r, g, b, a);
  }

  uint32_t PackAARRGGBB(uint32_t color) const {
    const uint8_t a = static_cast<uint8_t>((color >> 24) & 0xFF);
    const uint8_t r = static_cast<uint8_t>((color >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((color >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(color & 0xFF);
    return PackColor(r, g, b, a);
  }

  void DrawPoint(int x, int y, uint32_t color) {
    if (x >= m_Width || x < 0) return;
    if (y >= m_Height || y < 0) return;

    m_Framebuffer[x + y * m_Width] = color;
  }

  /**
   * Draw line with Bresenham algorithm
   */
  void DrawLine(const Vector2& startPos, const Vector2& endPos,
                uint32_t color) {
    auto drawLow = [this](int x0, int y0, int x1, int y1, uint32_t color) {
      int dx = x1 - x0, dy = y1 - y0;
      int yi = 1;
      if (dy < 0) {
        yi = -1;
        dy = -dy;
      }
      int d = (2 * dy) - dx;
      int y = y0;

      for (int x = x0; x < x1; ++x) {
        DrawPoint(x, y, color);
        if (d > 0) {
          y = y + yi;
          d = d + (2 * (dy - dx));
        } else {
          d = d + 2 * dy;
        }
      }
    };

    auto drawHigh = [this](int x0, int y0, int x1, int y1, uint32_t color) {
      int dx = x1 - x0, dy = y1 - y0;
      int xi = 1;
      if (dx < 0) {
        xi = -1;
        dx = -dx;
      }
      int d = (2 * dx) - dy;
      int x = x0;

      for (int y = y0; y < y1; ++y) {
        DrawPoint(x, y, color);
        if (d > 0) {
          x = x + xi;
          d = d + (2 * (dx - dy));
        } else {
          d = d + 2 * dx;
        }
      }
    };

    if (abs(endPos.y - startPos.y) < abs(endPos.x - startPos.x)) {
      if (startPos.x > endPos.x) {
        drawLow(endPos.x, endPos.y, startPos.x, startPos.y, color);
      } else {
        drawLow(startPos.x, startPos.y, endPos.x, endPos.y, color);
      }
    } else {
      if (startPos.y > endPos.y) {
        drawHigh(endPos.x, endPos.y, startPos.x, startPos.y, color);
      } else {
        drawHigh(startPos.x, startPos.y, endPos.x, endPos.y, color);
      }
    }
  }

  void TransformToScreen(Vector4& point) {
    point = m_ProjectionMatrix * (m_CameraMatrix * point);
    point.PerspectiveDivide();
    point = m_ViewportMatrix * point;
  }

  void SetupMatrices() {
    math::SetupCameraMatrix(m_CameraMatrix, m_Camera->eye, m_Camera->at,
                            m_Camera->up);
    math::SetupPerspectiveProjectionMatrix(m_ProjectionMatrix, m_Camera->fov,
                                           m_Camera->aspect, m_ZNear, m_ZFar);
    math::SetupViewportMatrix(m_ViewportMatrix, 0, 0, m_Width, m_Height,
                              m_ZNear, m_ZFar);
  }

  void BuildTriangle() {
    // Build some vertices for drawing triangle
    m_TriVerts[0] = {-1.0f, -1.0f, +0.0f};
    m_TriVerts[1] = {+1.0f, -1.0f, +0.0f};
    m_TriVerts[2] = {+0.0f, +1.0f, +0.0f};
  }

  void RenderTriangle() {
    Matrix4x4 rotateMat = Matrix4x4::identity;
    m_RotateRadian += 0.6f;
    rotateMat.RotateY(m_RotateRadian);

    Vector3 tri[3];
    for (int i = 0; i < 3; ++i) {
      const Vector3 rotated = rotateMat * m_TriVerts[i];
      Vector4 v = {rotated.x, rotated.y, rotated.z, 1.0f};
      TransformToScreen(v);
      tri[i].x = v.x, tri[i].y = v.y, tri[i].z = v.z;
    }

    const uint32_t fillColor = PackAARRGGBB(0xFF33AAFF);

    // #1
    FillTriangleBarycentric(tri[0], tri[1], tri[2], fillColor);

    // #2
    // DrawTri(tri[0], tri[1], tri[2], fillColor, true);

    // #3
    // FillTriangleTexture(tri[0], tri[1], tri[2], m_TgaTexture->PixelData());

    // Draw edges
    const uint32_t whiteColor = PackAARRGGBB(0xFFFFFFFF);
    DrawLine({tri[0].x, tri[0].y}, {tri[1].x, tri[1].y}, whiteColor);
    DrawLine({tri[1].x, tri[1].y}, {tri[2].x, tri[2].y}, whiteColor);
    DrawLine({tri[0].x, tri[0].y}, {tri[2].x, tri[2].y}, whiteColor);
  }

  void FillTriangleBarycentric(const Vector3& v0, const Vector3& v1,
                               const Vector3& v2, uint32_t color) {
    const float minX = std::min({v0.x, v1.x, v2.x});
    const float maxX = std::max({v0.x, v1.x, v2.x});
    const float minY = std::min({v0.y, v1.y, v2.y});
    const float maxY = std::max({v0.y, v1.y, v2.y});

    const int x0 = std::max(0, (int)std::floor(minX));
    const int y0 = std::max(0, (int)std::floor(minY));
    const int x1 = std::min(m_Width - 1, (int)std::ceil(maxX));
    const int y1 = std::min(m_Height - 1, (int)std::ceil(maxY));

    const Vector2 p0{v0.x, v0.y};
    const Vector2 p1{v1.x, v1.y};
    const Vector2 p2{v2.x, v2.y};
    const float area = math::EdgeFunction(p0, p1, v2.x, v2.y);
    if (std::abs(area) < 1e-6f) {
      return;
    }

    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const float px = (float)x + 0.5f;
        const float py = (float)y + 0.5f;

        const float w0 = math::EdgeFunction(p1, p2, px, py);
        const float w1 = math::EdgeFunction(p2, p0, px, py);
        const float w2 = math::EdgeFunction(p0, p1, px, py);

        const bool insideCCW = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f);
        const bool insideCW = (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
        if ((area > 0.0f && insideCCW) || (area < 0.0f && insideCW)) {
          DrawPoint(x, y, color);
        }
      }
    }
  }

  void DrawTri(const Vector3& v0, const Vector3& v1, const Vector3& v2,
               uint32_t color, bool cullBackface = true) {
    const float minX = std::min({v0.x, v1.x, v2.x});
    const float maxX = std::max({v0.x, v1.x, v2.x});
    const float minY = std::min({v0.y, v1.y, v2.y});
    const float maxY = std::max({v0.y, v1.y, v2.y});

    const int x0 = std::max(0, (int)std::floor(minX));
    const int y0 = std::max(0, (int)std::floor(minY));
    const int x1 = std::min(m_Width - 1, (int)std::ceil(maxX));
    const int y1 = std::min(m_Height - 1, (int)std::ceil(maxY));

    const Vector2 p0{v0.x, v0.y};
    const Vector2 p1{v1.x, v1.y};
    const Vector2 p2{v2.x, v2.y};
    const float area = math::EdgeFunction(p0, p1, v2.x, v2.y);
    if (std::abs(area) < 1e-6f) {
      return;
    }

    // Back-face culling (treat CCW as front-facing)
    if (cullBackface && area < 0.0f) {
      return;
    }

    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const float px = (float)x + 0.5f;
        const float py = (float)y + 0.5f;

        const float w0 = math::EdgeFunction(p1, p2, px, py);
        const float w1 = math::EdgeFunction(p2, p0, px, py);
        const float w2 = math::EdgeFunction(p0, p1, px, py);

        const bool insideCCW = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f);
        const bool insideCW = (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);

        const bool checkInside = cullBackface ? insideCCW
                                              : ((area > 0.0f && insideCCW) ||
                                                 (area < 0.0f && insideCW));

        if (checkInside) {
          // Zbuffer test
          const float invArea = 1.0f / area;
          const float b0 = w0 * invArea;
          const float b1 = w1 * invArea;
          const float b2 = w2 * invArea;

          const float z = b0 * v0.z + b1 * v1.z + b2 * v2.z;
          if (z < 0.0f || z > 1.0f) continue;

          const int idx = x + y * m_Width;
          if (z < m_ZBuffer[idx]) {
            m_ZBuffer[idx] = z;
            m_Framebuffer[idx] = color;
          }
        }
      }
    }
  }

  void BuildCube() {
    // Build some vertices for drawing cube
    m_CubeVerts[0] = {-1.0f, -1.0f, -1.0f};
    m_CubeVerts[1] = {-1.0f, +1.0f, -1.0f};
    m_CubeVerts[2] = {+1.0f, +1.0f, -1.0f};
    m_CubeVerts[3] = {+1.0f, -1.0f, -1.0f};

    m_CubeVerts[4] = {-1.0f, -1.0f, +1.0f};
    m_CubeVerts[5] = {-1.0f, +1.0f, +1.0f};
    m_CubeVerts[6] = {+1.0f, +1.0f, +1.0f};
    m_CubeVerts[7] = {+1.0f, -1.0f, +1.0f};
  }

  void RenderCubeLines() {
    Matrix4x4 rotateMat = Matrix4x4::identity;
    m_RotateRadian += 0.06f;
    rotateMat.RotateY(m_RotateRadian);

    Vector3 cube[8];
    for (int i = 0; i < 8; ++i) {
      const Vector3 rotated = rotateMat * m_CubeVerts[i];
      Vector4 v = {rotated.x, rotated.y, rotated.z, 1.0f};
      TransformToScreen(v);
      cube[i].x = v.x, cube[i].y = v.y, cube[i].z = v.z;
    }

    const uint32_t fillColor = PackAARRGGBB(0XFF33AAFF);
    // Front
    DrawTri(cube[0], cube[1], cube[2], fillColor, true);
    DrawTri(cube[0], cube[2], cube[3], fillColor, true);
    // Back
    DrawTri(cube[4], cube[5], cube[6], fillColor, true);
    DrawTri(cube[4], cube[6], cube[7], fillColor, true);
    // Top
    DrawTri(cube[1], cube[5], cube[6], fillColor, true);
    DrawTri(cube[1], cube[6], cube[7], fillColor, true);
    // Bottom
    DrawTri(cube[0], cube[4], cube[7], fillColor, true);
    DrawTri(cube[0], cube[7], cube[3], fillColor, true);
    // Right
    DrawTri(cube[3], cube[2], cube[6], fillColor, true);
    DrawTri(cube[3], cube[6], cube[7], fillColor, true);
    // Left
    DrawTri(cube[0], cube[1], cube[5], fillColor, true);
    DrawTri(cube[0], cube[5], cube[4], fillColor, true);

    const uint32_t whiteColor = PackAARRGGBB(0xFFFFFFFF);
    DrawLine({cube[0].x, cube[0].y}, {cube[1].x, cube[1].y}, whiteColor);
    DrawLine({cube[1].x, cube[1].y}, {cube[2].x, cube[2].y}, whiteColor);
    DrawLine({cube[2].x, cube[2].y}, {cube[3].x, cube[3].y}, whiteColor);
    DrawLine({cube[3].x, cube[3].y}, {cube[0].x, cube[0].y}, whiteColor);

    DrawLine({cube[4].x, cube[4].y}, {cube[5].x, cube[5].y}, whiteColor);
    DrawLine({cube[5].x, cube[5].y}, {cube[6].x, cube[6].y}, whiteColor);
    DrawLine({cube[6].x, cube[6].y}, {cube[7].x, cube[7].y}, whiteColor);
    DrawLine({cube[7].x, cube[7].y}, {cube[4].x, cube[4].y}, whiteColor);

    DrawLine({cube[0].x, cube[0].y}, {cube[4].x, cube[4].y}, whiteColor);
    DrawLine({cube[1].x, cube[1].y}, {cube[5].x, cube[5].y}, whiteColor);
    DrawLine({cube[2].x, cube[2].y}, {cube[6].x, cube[6].y}, whiteColor);
    DrawLine({cube[3].x, cube[3].y}, {cube[7].x, cube[7].y}, whiteColor);
  }

  void FillTriangleTexture(const Vector3& v0, const Vector3& v1,
                           const Vector3& v2, const Color* bitmap) {
    if (bitmap == nullptr || m_TgaTexture == nullptr) {
      return;
    }

    const TGAHeader* header = m_TgaTexture->Header();
    const int texWidth = (int)header->width;
    const int texHeight = (int)header->height;
    if (texWidth <= 0 || texHeight <= 0) {
      return;
    }

    const float minX = std::min({v0.x, v1.x, v2.x});
    const float maxX = std::max({v0.x, v1.x, v2.x});
    const float minY = std::min({v0.y, v1.y, v2.y});
    const float maxY = std::max({v0.y, v1.y, v2.y});

    const int x0 = std::max(0, (int)std::floor(minX));
    const int y0 = std::max(0, (int)std::floor(minY));
    const int x1 = std::min(m_Width - 1, (int)std::ceil(maxX));
    const int y1 = std::min(m_Height - 1, (int)std::ceil(maxY));
    if (x0 > x1 || y0 > y1) {
      return;
    }

    const Vector2 p0{v0.x, v0.y};
    const Vector2 p1{v1.x, v1.y};
    const Vector2 p2{v2.x, v2.y};
    const float area = math::EdgeFunction(p0, p1, v2.x, v2.y);
    if (std::abs(area) < 1e-6f) {
      return;
    }

    // Back-face culling (treat CCW as front-facing)
    if (area < 0.0f) {
      return;
    }

    const float invArea = 1.0f / area;
    const int texMaxX = texWidth - 1;
    const int texMaxY = texHeight - 1;

    const float stepW0X = p2.y - p1.y;
    const float stepW1X = p0.y - p2.y;
    const float stepW2X = p1.y - p0.y;

    const float stepW0Y = p1.x - p2.x;
    const float stepW1Y = p2.x - p0.x;
    const float stepW2Y = p0.x - p1.x;

    const float startX = (float)x0 + 0.5f;
    const float startY = (float)y0 + 0.5f;

    float w0Row = math::EdgeFunction(p1, p2, startX, startY);
    float w1Row = math::EdgeFunction(p2, p0, startX, startY);
    float w2Row = math::EdgeFunction(p0, p1, startX, startY);

    for (int y = y0; y <= y1; ++y) {
      float w0 = w0Row;
      float w1 = w1Row;
      float w2 = w2Row;
      int idx = x0 + y * m_Width;

      for (int x = x0; x <= x1; ++x, ++idx) {
        if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
          const float b0 = w0 * invArea;
          const float b1 = w1 * invArea;
          const float b2 = w2 * invArea;

          const float z = b0 * v0.z + b1 * v1.z + b2 * v2.z;
          if (z >= 0.0f && z <= 1.0f && z < m_ZBuffer[idx]) {
            m_ZBuffer[idx] = z;

            float u = b1 + (0.5f * b2);
            float v = b0 + b1;
            u = std::max(0.0f, std::min(1.0f, u));
            v = std::max(0.0f, std::min(1.0f, v));

            const int texX = std::min(
                texMaxX, std::max(0, (int)(u * (float)texMaxX + 0.5f)));
            const int texY = std::min(
                texMaxY, std::max(0, (int)(v * (float)texMaxY + 0.5f)));
            const Color& sample = bitmap[texX + texY * texWidth];

            // Map logical color channels to the selected framebuffer format.
            m_Framebuffer[idx] = PackColor(sample.r, sample.g, sample.b, sample.a);
          }
        }

        w0 += stepW0X;
        w1 += stepW1X;
        w2 += stepW2X;
      }

      w0Row += stepW0Y;
      w1Row += stepW1Y;
      w2Row += stepW2Y;
    }
  }

  Color SampleTexture(const TGA* texture, float u, float v) {
    if (texture == nullptr || texture->Header() == nullptr ||
        texture->PixelData() == nullptr) {
      return Color{};
    }

    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));
    v = 1.0f - v;

    const int texWidth = (int)texture->Header()->width;
    const int texHeight = (int)texture->Header()->height;
    if (texWidth <= 0 || texHeight <= 0) {
      return Color{};
    }

    const int tx =
        std::min(texWidth - 1, std::max(0, (int)(u * (float)(texWidth - 1))));
    const int ty =
        std::min(texHeight - 1, std::max(0, (int)(v * (float)(texHeight - 1))));
    return texture->PixelData()[tx + ty * texWidth];
  }

  Color BilinearSampleTexture(const TGA* texture, float u, float v) {
    if (texture == nullptr || texture->Header() == nullptr ||
        texture->PixelData() == nullptr) {
      return Color{};
    }

    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));
    v = 1.0f - v;

    const int w = (int)texture->Header()->width;
    const int h = (int)texture->Header()->height;
    if (w <= 0 || h <= 0) {
      return Color{};
    }

    const Color* pixel = texture->PixelData();

    const float x = u * (float)(w - 1);
    const float y = v * (float)(h - 1);
    const int x0 = std::max(0, std::min(w - 1, (int)std::floor(x)));
    const int y0 = std::max(0, std::min(h - 1, (int)std::floor(y)));
    const int x1 = std::min(w - 1, x0 + 1);
    const int y1 = std::min(h - 1, y0 + 1);
    const float fx = x - (float)x0;
    const float fy = y - (float)y0;

    const Color& c00 = pixel[x0 + y0 * w];  // top-left
    const Color& c10 = pixel[x1 + y0 * w];  // top-right
    const Color& c01 = pixel[x0 + y1 * w];  // bottom-left
    const Color& c11 = pixel[x1 + y1 * w];  // bottom-right

    auto blendChannel = [fx, fy](uint8_t cc00, uint8_t cc10, uint8_t cc01,
                                 uint8_t cc11) -> uint8_t {
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

  void BuildSimpleCubeMesh() {
    m_SimpleCubeMesh = new SimpleMesh();
    auto& verts = m_SimpleCubeMesh->verts;
    verts.reserve(24);
    verts = {
        // Front face (z = -1)
        {-1.0f, -1.0f, -1.0f},
        {-1.0f, 1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f},
        {1.0f, -1.0f, -1.0f},

        // Back face (z = +1)
        {1.0f, -1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f},
        {-1.0f, -1.0f, 1.0f},

        // Right face (x = +1)
        {1.0f, -1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f},

        // Left face (x = -1)
        {-1.0f, -1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f},
        {-1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, -1.0f},

        // Top face (y = +1)
        {-1.0f, 1.0f, -1.0f},
        {-1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, -1.0f},

        // Bottom face (y = -1)
        {-1.0f, -1.0f, 1.0f},
        {-1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, 1.0f},
    };

    auto& indices = m_SimpleCubeMesh->indices;
    indices.reserve(24);
    indices = {// front
               0, 2, 1, 0, 3, 2,
               // back
               4, 6, 5, 4, 7, 6,
               // right
               8, 10, 9, 8, 11, 10,
               // left
               12, 14, 13, 12, 15, 14,
               // top
               16, 18, 17, 16, 19, 18,
               // bottom
               20, 22, 21, 20, 23, 22};

    auto& uvs = m_SimpleCubeMesh->uvs;
    uvs.reserve(24);
    uvs = {
        // front
        {0.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        // back
        {0.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        // right
        {0.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        // left
        {0.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        // top
        {0.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        // bottom
        {0.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
    };

    m_SimpleCubeMesh->tga = m_TgaTexture;

    m_SimpleCubeClipZs.resize(m_SimpleCubeMesh->verts.size());
    m_SimpleCubeTransformVerts.resize(m_SimpleCubeMesh->verts.size());
    m_SimpleCubeInvWs.resize(m_SimpleCubeMesh->verts.size());
  }

  void ApplyPerspectiveCorrect(Matrix4x4& modelMat, float deltaMs) {
    // Transform vertices to clip space and store clip Z and inverse W
    // for perspective-correct interpolation
    for (size_t i = 0; i < m_SimpleCubeMesh->verts.size(); ++i) {
      Vector4 v = {m_SimpleCubeMesh->verts[i].x, m_SimpleCubeMesh->verts[i].y,
                   m_SimpleCubeMesh->verts[i].z, 1.0f};
      v = modelMat * v;

      Vector4 clip = m_ProjectionMatrix * (m_CameraMatrix * v);

      float invW = (clip.w != 0.0f) ? (1.0f / clip.w) : 1.0f;
      m_SimpleCubeInvWs[i] = invW;
      // Store NDC depth for depth-test interpolation in screen space.
      m_SimpleCubeClipZs[i] = clip.z * invW;

      Vector4 ndc = {clip.x * invW, clip.y * invW, clip.z * invW, 1.0f};
      ndc = m_ViewportMatrix * ndc;

      m_SimpleCubeTransformVerts[i] = {ndc.x, ndc.y, ndc.z};
    }
  }

  void RenderCubeMesh(float deltaMs) {
    if (m_SimpleCubeMesh == nullptr || m_SimpleCubeMesh->tga == nullptr) {
      return;
    }

    m_SimpleCubeRotateRadian += 2.8f;
    if (m_SimpleCubeRotateRadian > 360.0f) {
      m_SimpleCubeRotateRadian -= 360.0f;
    } else if (m_SimpleCubeRotateRadian < -360.0f) {
      m_SimpleCubeRotateRadian += 360.0f;
    }

    Matrix4x4 modelMat = Matrix4x4::identity;
    modelMat.Rotate(m_SimpleCubeRotateRadian, m_SimpleCubeRotateRadian,
                    m_SimpleCubeRotateRadian);

    ApplyPerspectiveCorrect(modelMat, deltaMs);

    for (size_t idx = 0; idx + 2 < m_SimpleCubeMesh->indices.size(); idx += 3) {
      uint32_t i0 = m_SimpleCubeMesh->indices[idx];
      uint32_t i1 = m_SimpleCubeMesh->indices[idx + 1];
      uint32_t i2 = m_SimpleCubeMesh->indices[idx + 2];
      const Vector3& v0 = m_SimpleCubeTransformVerts[i0];
      const Vector3& v1 = m_SimpleCubeTransformVerts[i1];
      const Vector3& v2 = m_SimpleCubeTransformVerts[i2];

      // FillTriangleBarycentric(v0, v1, v2, 0xFF33AAFF);

      // FillTriangleTexture(v0, v1, v2, m_SimpleCubeMesh->tga->PixelData());

      const Vector2& uv0 = m_SimpleCubeMesh->uvs[i0];
      const Vector2& uv1 = m_SimpleCubeMesh->uvs[i1];
      const Vector2& uv2 = m_SimpleCubeMesh->uvs[i2];
      const float invW0 = m_SimpleCubeInvWs[i0];
      const float invW1 = m_SimpleCubeInvWs[i1];
      const float invW2 = m_SimpleCubeInvWs[i2];
      const float clipZ0 = m_SimpleCubeClipZs[i0];
      const float clipZ1 = m_SimpleCubeClipZs[i1];
      const float clipZ2 = m_SimpleCubeClipZs[i2];

      // #3
      DrawTexturedTriangle(v0, v1, v2, uv0, uv1, uv2, invW0, invW1, invW2,
                           clipZ0, clipZ1, clipZ2, m_SimpleCubeMesh->tga,
                           m_ZBuffer);

      // [Optional] Draw wireframe edges
      const uint32_t whiteColor = PackAARRGGBB(0xFFFFFFFF);
      DrawLine({v0.x, v0.y}, {v1.x, v1.y}, whiteColor);
      DrawLine({v1.x, v1.y}, {v2.x, v2.y}, whiteColor);
      DrawLine({v0.x, v0.y}, {v2.x, v2.y}, whiteColor);
    }
  }

  void DrawTexturedTriangle(const Vector3& v0, const Vector3& v1,
                            const Vector3& v2, const Vector2& uv0,
                            const Vector2& uv1, const Vector2& uv2, float invW0,
                            float invW1, float invW2, float clipZ0,
                            float clipZ1, float clipZ2, const TGA* texture,
                            float* m_ZBuffer) {
    if (texture == nullptr || m_ZBuffer == nullptr) {
      return;
    }

    Vector2 a = {v0.x, v0.y};
    Vector2 b = {v1.x, v1.y};
    Vector2 c = {v2.x, v2.y};

    float minX = std::min({a.x, b.x, c.x});
    float maxX = std::max({a.x, b.x, c.x});
    float minY = std::min({a.y, b.y, c.y});
    float maxY = std::max({a.y, b.y, c.y});

    int x0 = std::max(0, (int)std::floor(minX));
    int y0 = std::max(0, (int)std::floor(minY));
    int x1 = std::min(m_Width - 1, (int)std::ceil(maxX));
    int y1 = std::min(m_Height - 1, (int)std::ceil(maxY));
    if (x0 > x1 || y0 > y1) {
      return;
    }

    // Back-face culling (treat CCW as front-facing)
    const float area = math::EdgeFunction(a, b, c.x, c.y);
    if (area <= 1e-6f) {
      return;
    }
    const float invArea = 1.0f / area;

    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        float px = x + 0.5f;
        float py = y + 0.5f;
        float w0 = math::EdgeFunction(b, c, px, py);
        float w1 = math::EdgeFunction(c, a, px, py);
        float w2 = math::EdgeFunction(a, b, px, py);

        // Back-face culling (treat CCW as front-facing)
        if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
          continue;
        }

        const float b0 = w0 * invArea;
        const float b1 = w1 * invArea;
        const float b2 = w2 * invArea;

        const float denom = (b0 * invW0) + (b1 * invW1) + (b2 * invW2);
        if (std::abs(denom) <= 1e-8f) {
          continue;
        }
        const float invDenom = 1.0f / denom;

        const float u =
            (uv0.x * invW0 * b0 + uv1.x * invW1 * b1 + uv2.x * invW2 * b2) *
            invDenom;
        const float v =
            (uv0.y * invW0 * b0 + uv1.y * invW1 * b1 + uv2.y * invW2 * b2) *
            invDenom;
        // Depth is already in NDC at vertices; interpolate linearly in screen
        // space.
        const float z = (clipZ0 * b0 + clipZ1 * b1 + clipZ2 * b2);
        if (z < 0.0f || z > 1.0f) {
          continue;
        }

        int depthIndex = x + y * m_Width;
        if (z >= m_ZBuffer[depthIndex]) {
          continue;
        }
        Color color = BilinearSampleTexture(texture, u, v);

        if ((static_cast<uint32_t>(color) >> 24) == 0) {
          continue;
        }

        m_ZBuffer[depthIndex] = z;
        DrawPoint(x, y, PackColor(color.r, color.g, color.b, color.a));
      }
    }
  }

  // TODO: 8. Other shaders; Flat, Gouraud, Phong, Blinn-Phong, etc.

  // TODO: 9. OBJ mesh loading and rendering
  void BuildBunnyMesh() {
    m_BunnyMesh = m_TextureLoader->LoadSimpleMeshFromObj("../resources/simplified_stanford_bunny.obj");
    if(m_BunnyMesh != nullptr) {
      m_BunnyTransformVerts.resize(m_BunnyMesh->verts.size());
    }
  }

  void RenderBunnyMesh() {
    if(m_BunnyMesh == nullptr) {
      return;
    }

    Matrix4x4 modelMat = Matrix4x4::identity;
    m_RotateRadian += 0.06f;
    modelMat.RotateY(m_RotateRadian);

    for(size_t i = 0; i < m_BunnyMesh->verts.size(); ++i) {
      const Vector3& src = m_BunnyMesh->verts[i];
      const Vector3 rotated = modelMat * src;
      Vector4 v = { rotated.x, rotated.y, rotated.z, 1.0f };
      TransformToScreen(v);
      m_BunnyTransformVerts[i] = { v.x, v.y, v.z };
    }

    const uint32_t fillColor = PackAARRGGBB(0xFF33AAFF);
    for(size_t idx = 0; idx + 2 < m_BunnyMesh->indices.size(); idx+=3) {
      uint32_t i0 = m_BunnyMesh->indices[idx];
      uint32_t i1 = m_BunnyMesh->indices[idx+1];
      uint32_t i2 = m_BunnyMesh->indices[idx+2];
      const Vector3& v0 = m_BunnyTransformVerts[i0];
      const Vector3& v1 = m_BunnyTransformVerts[i1];
      const Vector3& v2 = m_BunnyTransformVerts[i2];

      DrawTri(v0, v1, v2, fillColor, true);
    }
  }

  // TODO: 10. Camera movement and interaction

 private:
  int m_Width{0};
  int m_Height{0};
  uint32_t* m_Framebuffer{nullptr};
  WorldCamera* m_Camera{nullptr};

  Matrix4x4 m_ViewportMatrix, m_ProjectionMatrix, m_CameraMatrix;

  SDL_Renderer* m_Renderer{nullptr};
  SDL_Texture* m_MainTexture{nullptr};
  uint32_t m_FramebufferFormatEnum{SDL_PIXELFORMAT_ARGB8888};
  SDL_PixelFormat* m_FramebufferFormat{nullptr};
  TextureLoader* m_TextureLoader{nullptr};
  float* m_ZBuffer{nullptr};

  const float m_ZNear{0.1f}, m_ZFar{10.0f};

  float m_RotateRadian{0.0f};

  // Start Draw Tri
  Vector3 m_TriVerts[3];
  // End Draw Tri

  // Start Draw Cube
  Vector3 m_CubeVerts[8];
  // End Draw Cube

  // Start Draw Textured Tri
  TGA* m_TgaTexture{nullptr};
  // End Draw Textured Tri

  // Start Render Mesh
  SimpleMesh* m_SimpleCubeMesh{nullptr};
  std::vector<Vector3> m_SimpleCubeTransformVerts;
  std::vector<float> m_SimpleCubeClipZs;
  std::vector<float> m_SimpleCubeInvWs;
  float m_SimpleCubeRotateRadian{0.0f};
  // End Render Mesh

  // Start Render Bunny Mesh
  SimpleMesh* m_BunnyMesh{nullptr};
  std::vector<Vector3> m_BunnyTransformVerts;
  // End Render Bunny Mesh
};
