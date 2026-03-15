#pragma once

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "DebugDumpApi.hpp"
#include "Mesh.hpp"
#include "TextureLoader.hpp"
#include "WorldCamera.hpp"

class Renderer
{
public:
  Renderer(SDL_Window *window, int width, int height)
      : m_Width(width), m_Height(height)
  {
    m_Framebuffer = new uint32_t[m_Width * m_Height];
    m_Camera = new WorldCamera();
    m_ZBuffer = new float[m_Width * m_Height];
    std::fill(m_ZBuffer, m_ZBuffer + m_Width * m_Height, 1.0f);

    m_Renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (m_Renderer == nullptr)
    {
      LogF("SDL_CreateRenderer failed: %s", SDL_GetError());
      SDL_assert(false);
      return;
    }

    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(m_Renderer, &info) == 0)
    {
      LogF("Renderer backend: %s", info.name);
      if (info.num_texture_formats > 0)
      {
        m_FramebufferFormatEnum = info.texture_formats[0];
      }
      for (uint32_t i = 0; i < info.num_texture_formats; ++i)
      {
        LogF("Supported[%u]: %s", i,
             SDL_GetPixelFormatName(info.texture_formats[i]));
      }
    }

    if (SDL_ISPIXELFORMAT_FOURCC(m_FramebufferFormatEnum) ||
        SDL_BYTESPERPIXEL(m_FramebufferFormatEnum) !=
            static_cast<int>(sizeof(uint32_t)))
    {
      LogF("Unsupported primary format (%s). Fallback to ARGB8888.",
           SDL_GetPixelFormatName(m_FramebufferFormatEnum));
      m_FramebufferFormatEnum = SDL_PIXELFORMAT_ARGB8888;
    }

    m_FramebufferFormat = SDL_AllocFormat(m_FramebufferFormatEnum);
    if (m_FramebufferFormat == nullptr)
    {
      LogF("SDL_AllocFormat failed for %s: %s",
           SDL_GetPixelFormatName(m_FramebufferFormatEnum), SDL_GetError());
      m_FramebufferFormatEnum = SDL_PIXELFORMAT_ARGB8888;
      m_FramebufferFormat = SDL_AllocFormat(m_FramebufferFormatEnum);
    }
    if (m_FramebufferFormat != nullptr)
    {
      LogF("Selected framebuffer format: %s (bpp=%u)",
           SDL_GetPixelFormatName(m_FramebufferFormatEnum),
           m_FramebufferFormat->BitsPerPixel);
    }

    m_MainTexture =
        SDL_CreateTexture(m_Renderer, m_FramebufferFormatEnum,
                          SDL_TEXTUREACCESS_STREAMING, m_Width, m_Height);
    if (m_MainTexture == nullptr)
    {
      LogF("SDL_CreateTexture failed for %s: %s",
           SDL_GetPixelFormatName(m_FramebufferFormatEnum), SDL_GetError());
      SDL_assert(false);
      return;
    }

    m_Camera->aspect = (float)m_Width / m_Height;
    m_Camera->fov = 45.0f;

    SetupMatrices();

    m_TextureLoader = new TextureLoader("resources");

    BuildSimpleCubeMesh();
    // BuildBunnyMesh();
  }

  ~Renderer()
  {
    delete[] m_Framebuffer;
    delete[] m_ZBuffer;
    delete m_Camera;
    delete m_SimpleCubeMesh;
    delete m_BunnyMesh;
    delete m_TextureLoader;
    if (m_FramebufferFormat != nullptr)
    {
      SDL_FreeFormat(m_FramebufferFormat);
      m_FramebufferFormat = nullptr;
    }
    SDL_DestroyTexture(m_MainTexture);
    SDL_DestroyRenderer(m_Renderer);
  }

  void Render(double delta)
  {
    ClearBuffers();

    // RenderCubeMesh(static_cast<float>(delta));
    RenderBunnyMesh();

    SDL_UpdateTexture(m_MainTexture, nullptr, m_Framebuffer,
                      m_Width * static_cast<int>(sizeof(uint32_t)));
    SDL_RenderCopy(m_Renderer, m_MainTexture, nullptr, nullptr);
    SDL_RenderPresent(m_Renderer);
    debugdump::DumpFramebufferIfRequested(m_Framebuffer, m_Width, m_Height,
                                          m_FramebufferFormatEnum);
    SDL_Delay(32);
  }

  void HandleKeyInput(SDL_Keycode key)
  {
    switch (key)
    {
    case SDLK_UP:
    {
      m_Camera->fov++;
      m_ProjectionMatrix = Matrix4x4::identity;
      math::SetupPerspectiveProjectionMatrix(m_ProjectionMatrix,
                                             m_Camera->fov, m_Camera->aspect, m_ZNear, m_ZFar);
      break;
    }
    case SDLK_DOWN:
    {
      m_Camera->fov--;
      m_ProjectionMatrix = Matrix4x4::identity;
      math::SetupPerspectiveProjectionMatrix(m_ProjectionMatrix,
                                             m_Camera->fov, m_Camera->aspect, m_ZNear, m_ZFar);
      break;
    }
    case SDLK_RIGHT:
    {
      m_Camera->eye.x += 0.1f;
      m_CameraMatrix = Matrix4x4::identity;
      math::SetupCameraMatrix(m_CameraMatrix, m_Camera->eye,
                              m_Camera->at, m_Camera->up);
      break;
    }
    case SDLK_LEFT:
    {
      m_Camera->eye.x -= 0.1f;
      m_CameraMatrix = Matrix4x4::identity;
      math::SetupCameraMatrix(m_CameraMatrix, m_Camera->eye,
                              m_Camera->at, m_Camera->up);
      break;
    }
    case SDLK_r:
    {
      // Reset camera
      m_Camera->eye = {2.2f, 1.2f, -6.5f};
      m_Camera->at = {0.0f, 0.0f, 0.0f};
      m_Camera->up = {0.0f, 1.0f, 0.0f};
      m_Camera->fov = 45.0f;
      SetupMatrices();
      break;
    }
    default:
    {
      break;
    }
    }
  }

private:
  void ClearBuffers()
  {
    std::fill(m_Framebuffer, m_Framebuffer + m_Width * m_Height, 0);
    std::fill(m_ZBuffer, m_ZBuffer + m_Width * m_Height, 1.0f);
  }

  uint32_t PackColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) const
  {
    if (m_FramebufferFormat == nullptr)
    {
      return (static_cast<uint32_t>(a) << 24) |
             (static_cast<uint32_t>(r) << 16) |
             (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    }
    return SDL_MapRGBA(m_FramebufferFormat, r, g, b, a);
  }

  uint32_t PackAARRGGBB(uint32_t color) const
  {
    const uint8_t a = static_cast<uint8_t>((color >> 24) & 0xFF);
    const uint8_t r = static_cast<uint8_t>((color >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((color >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(color & 0xFF);
    return PackColor(r, g, b, a);
  }

  void DrawPoint(int x, int y, float z, uint32_t color)
  {
    if (x >= m_Width || x < 0)
      return;
    if (y >= m_Height || y < 0)
      return;

    if (m_ZBuffer[x + y * m_Width] < z)
    {
      return;
    }

    m_Framebuffer[x + y * m_Width] = color;
    m_ZBuffer[x + y * m_Width] = z;
  }

  void TransformToScreen(Vector4 &point)
  {
    point = m_ProjectionMatrix * (m_CameraMatrix * point);
    point.PerspectiveDivide();
    point = m_ViewportMatrix * point;
  }

  bool ProjectWorldPointToScreen(const Vector3 &point, Vector3 &out)
  {
    Vector4 clip = m_ProjectionMatrix *
                   (m_CameraMatrix * Vector4(point.x, point.y, point.z, 1.0f));
    if (clip.w <= 1e-6f)
    {
      return false;
    }

    clip.PerspectiveDivide();
    if (!std::isfinite(clip.x) || !std::isfinite(clip.y) ||
        !std::isfinite(clip.z))
    {
      return false;
    }
    if (clip.z < 0.0f || clip.z > 1.0f)
    {
      return false;
    }

    clip = m_ViewportMatrix * clip;
    out = {clip.x, clip.y, clip.z};
    return true;
  }

  Vector3 TransformWorldPointToView(const Vector3 &point) const
  {
    const Vector4 view =
        m_CameraMatrix * Vector4(point.x, point.y, point.z, 1.0f);
    return {view.x, view.y, view.z};
  }

  bool IsFrontFacingViewSpace(const Vector3 &view0, const Vector3 &view1,
                              const Vector3 &view2) const
  {
    const Vector3 edge0 = view1 - view0;
    const Vector3 edge1 = view2 - view0;
    const Vector3 normal = math::CrossProduct(edge0, edge1);
    if (math::DotProduct(normal, normal) <= 1e-8f)
    {
      return false;
    }

    const Vector3 centroid = (view0 + view1 + view2) * (1.0f / 3.0f);
    const Vector3 toCamera = centroid * -1.0f;
    return math::DotProduct(normal, toCamera) < 0.0f;
  }

  uint32_t ShadeBunnyFace(const Vector3 &world0, const Vector3 &world1,
                          const Vector3 &world2) const
  {
    const Vector3 edge0 = world1 - world0;
    const Vector3 edge1 = world2 - world0;
    Vector3 normal = math::CrossProduct(edge0, edge1);
    const float normalLengthSq = math::DotProduct(normal, normal);
    if (normalLengthSq <= 1e-8f)
    {
      return PackAARRGGBB(0xFFB0D0E8);
    }

    normal = normal.Normalize();
    const Vector3 lightDir = Vector3(-0.35f, 0.85f, -0.40f).Normalize();
    const float diffuse = std::max(0.0f, math::DotProduct(normal, lightDir));
    const float light = 0.22f + diffuse * 0.78f;

    const uint8_t r = static_cast<uint8_t>(110.0f + light * 90.0f);
    const uint8_t g = static_cast<uint8_t>(150.0f + light * 80.0f);
    const uint8_t b = static_cast<uint8_t>(175.0f + light * 60.0f);
    return PackColor(r, g, b, 255);
  }

  Vector3 TransformDirection(const Matrix4x4 &matrix,
                             const Vector3 &direction) const
  {
    const Vector4 transformed =
        matrix * Vector4(direction.x, direction.y, direction.z, 0.0f);
    Vector3 result{transformed.x, transformed.y, transformed.z};
    if (math::DotProduct(result, result) <= 1e-8f)
    {
      return {0.0f, 1.0f, 0.0f};
    }
    return result.Normalize();
  }

  uint32_t ShadeMeshNormals(const Vector3 &normal0, const Vector3 &normal1,
                            const Vector3 &normal2,
                            const Matrix4x4 &modelMat) const
  {
    Vector3 normal = (normal0 + normal1 + normal2) * (1.0f / 3.0f);
    if (math::DotProduct(normal, normal) <= 1e-8f)
    {
      normal = {0.0f, 1.0f, 0.0f};
    }

    normal = TransformDirection(modelMat, normal);
    const Vector3 lightDir = Vector3(-0.45f, 0.80f, -0.40f).Normalize();
    const float diffuse = std::max(0.0f, math::DotProduct(normal, lightDir));
    const float light = 0.20f + diffuse * 0.80f;

    const uint8_t r = static_cast<uint8_t>(70.0f + light * 140.0f);
    const uint8_t g = static_cast<uint8_t>(95.0f + light * 115.0f);
    const uint8_t b = static_cast<uint8_t>(140.0f + light * 95.0f);
    return PackColor(r, g, b, 255);
  }

  void SetupMatrices()
  {
    math::SetupCameraMatrix(m_CameraMatrix, m_Camera->eye, m_Camera->at,
                            m_Camera->up);
    math::SetupPerspectiveProjectionMatrix(m_ProjectionMatrix, m_Camera->fov,
                                           m_Camera->aspect, m_ZNear, m_ZFar);
    math::SetupViewportMatrix(m_ViewportMatrix, 0, 0, m_Width, m_Height,
                              m_ZNear, m_ZFar);
  }

  void DrawTri(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
               uint32_t color, bool cullBackface = true)
  {
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
    if (std::abs(area) < 1e-6f)
    {
      return;
    }

    // Back-face culling (treat CCW as front-facing)
    if (cullBackface && area < 0.0f)
    {
      return;
    }

    for (int y = y0; y <= y1; ++y)
    {
      for (int x = x0; x <= x1; ++x)
      {
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

        if (checkInside)
        {
          // Zbuffer test
          const float invArea = 1.0f / area;
          const float b0 = w0 * invArea;
          const float b1 = w1 * invArea;
          const float b2 = w2 * invArea;

          const float z = b0 * v0.z + b1 * v1.z + b2 * v2.z;
          if (z < 0.0f || z > 1.0f)
            continue;

          const int idx = x + y * m_Width;
          if (z < m_ZBuffer[idx])
          {
            m_ZBuffer[idx] = z;
            m_Framebuffer[idx] = color;
          }
        }
      }
    }
  }

  Color SampleTexture(const TGA *texture, float u, float v)
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

  Color BilinearSampleTexture(const TGA *texture, float u, float v)
  {
    if (texture == nullptr || texture->Header() == nullptr ||
        texture->PixelData() == nullptr)
    {
      return Color{};
    }

    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));
    v = 1.0f - v;

    const int w = (int)texture->Header()->width;
    const int h = (int)texture->Header()->height;
    if (w <= 0 || h <= 0)
    {
      return Color{};
    }

    const Color *pixel = texture->PixelData();

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

  // Render simple cube mesh
  void BuildSimpleCubeMesh()
  {
    m_SimpleCubeMesh =
        m_TextureLoader->LoadSimpleMeshFromObj("../resources/cube.obj");
    if (m_SimpleCubeMesh == nullptr || m_SimpleCubeMesh->verts.empty())
    {
      LogF("Failed to load cube.obj");
      return;
    }

    Vector3 min = m_SimpleCubeMesh->verts[0];
    Vector3 max = m_SimpleCubeMesh->verts[0];
    for (const Vector3 &v : m_SimpleCubeMesh->verts)
    {
      min.x = std::min(min.x, v.x);
      min.y = std::min(min.y, v.y);
      min.z = std::min(min.z, v.z);
      max.x = std::max(max.x, v.x);
      max.y = std::max(max.y, v.y);
      max.z = std::max(max.z, v.z);
    }

    m_SimpleCubeCenter = {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f,
                          (min.z + max.z) * 0.5f};
    const Vector3 size = max - min;
    const float maxExtent = std::max({size.x, size.y, size.z});
    if (maxExtent > 1e-6f)
    {
      m_SimpleCubeScale = 3.0f / maxExtent;
    }

    m_SimpleCubeNormalizedVerts.resize(m_SimpleCubeMesh->verts.size());
    for (size_t i = 0; i < m_SimpleCubeMesh->verts.size(); ++i)
    {
      m_SimpleCubeNormalizedVerts[i] =
          (m_SimpleCubeMesh->verts[i] - m_SimpleCubeCenter) *
          m_SimpleCubeScale;
    }
  }

  void RenderCubeMesh(float deltaMs)
  {
    if (m_SimpleCubeMesh == nullptr)
    {
      return;
    }
    const bool debugPoints = debugdump::IsPointCloudModeEnabled();

    m_SimpleCubeRotateRadian += deltaMs * 0.02f;
    if (m_SimpleCubeRotateRadian > 360.0f)
    {
      m_SimpleCubeRotateRadian -= 360.0f;
    }
    else if (m_SimpleCubeRotateRadian < -360.0f)
    {
      m_SimpleCubeRotateRadian += 360.0f;
    }

    Matrix4x4 modelMat = Matrix4x4::identity;
    modelMat.Rotate(m_SimpleCubeRotateRadian, m_SimpleCubeRotateRadian,
                    m_SimpleCubeRotateRadian);

    for (size_t idx = 0; idx + 2 < m_SimpleCubeMesh->indices.size(); idx += 3)
    {
      uint32_t i0 = m_SimpleCubeMesh->indices[idx];
      uint32_t i1 = m_SimpleCubeMesh->indices[idx + 1];
      uint32_t i2 = m_SimpleCubeMesh->indices[idx + 2];
      const Vector3 world0 = modelMat * m_SimpleCubeNormalizedVerts[i0];
      const Vector3 world1 = modelMat * m_SimpleCubeNormalizedVerts[i1];
      const Vector3 world2 = modelMat * m_SimpleCubeNormalizedVerts[i2];

      Vector3 screen0{};
      Vector3 screen1{};
      Vector3 screen2{};
      if (!ProjectWorldPointToScreen(world0, screen0) ||
          !ProjectWorldPointToScreen(world1, screen1) ||
          !ProjectWorldPointToScreen(world2, screen2))
      {
        continue;
      }

      if (debugPoints)
      {
        DrawPoint(static_cast<int>(screen0.x), static_cast<int>(screen0.y),
                  screen0.z, PackAARRGGBB(0xFFFFFFFF));
        DrawPoint(static_cast<int>(screen1.x), static_cast<int>(screen1.y),
                  screen1.z, PackAARRGGBB(0xFFFFFFFF));
        DrawPoint(static_cast<int>(screen2.x), static_cast<int>(screen2.y),
                  screen2.z, PackAARRGGBB(0xFFFFFFFF));
        continue;
      }

      const uint32_t fillColor =
          ShadeMeshNormals(m_SimpleCubeMesh->normals[i0],
                           m_SimpleCubeMesh->normals[i1],
                           m_SimpleCubeMesh->normals[i2], modelMat);
      DrawTri(screen0, screen1, screen2, fillColor, false);
    }
  }

  // TODO: 8. Other shaders; Flat, Gouraud, Phong, Blinn-Phong, etc.

  void BuildBunnyMesh()
  {
    m_BunnyMesh = m_TextureLoader->LoadSimpleMeshFromObj("../resources/bunny.obj");
    if (m_BunnyMesh != nullptr)
    {
      if (!m_BunnyMesh->verts.empty())
      {
        Vector3 min = m_BunnyMesh->verts[0];
        Vector3 max = m_BunnyMesh->verts[0];
        for (const Vector3 &v : m_BunnyMesh->verts)
        {
          min.x = std::min(min.x, v.x);
          min.y = std::min(min.y, v.y);
          min.z = std::min(min.z, v.z);
          max.x = std::max(max.x, v.x);
          max.y = std::max(max.y, v.y);
          max.z = std::max(max.z, v.z);
        }
        m_BunnyCenter = {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f,
                         (min.z + max.z) * 0.5f};
        const Vector3 size = max - min;
        const float maxExtent = std::max({size.x, size.y, size.z});
        if (maxExtent > 1e-6f)
        {
          m_BunnyScale = 4.0f / maxExtent;
        }
      }

      m_BunnyNormalizedVerts.resize(m_BunnyMesh->verts.size());
      for (size_t i = 0; i < m_BunnyMesh->verts.size(); ++i)
      {
        m_BunnyNormalizedVerts[i] =
            (m_BunnyMesh->verts[i] - m_BunnyCenter) * m_BunnyScale;
      }
    }
  }

  void RenderBunnyMesh()
  {
    if (m_BunnyMesh == nullptr)
    {
      return;
    }
    const bool debugPoints = debugdump::IsPointCloudModeEnabled();

    Matrix4x4 modelMat = Matrix4x4::identity;
    m_RotateRadian += 0.45f;
    if (m_RotateRadian > 360.0f)
    {
      m_RotateRadian -= 360.0f;
    }
    modelMat.Rotate(0.0f, m_RotateRadian, 0.0f);

    for (size_t idx = 0; idx + 2 < m_BunnyMesh->indices.size(); idx += 3)
    {
      uint32_t i0 = m_BunnyMesh->indices[idx];
      uint32_t i1 = m_BunnyMesh->indices[idx + 1];
      uint32_t i2 = m_BunnyMesh->indices[idx + 2];

      const Vector3 world0 = modelMat * m_BunnyNormalizedVerts[i0];
      const Vector3 world1 = modelMat * m_BunnyNormalizedVerts[i1];
      const Vector3 world2 = modelMat * m_BunnyNormalizedVerts[i2];

      Vector3 screen0{};
      Vector3 screen1{};
      Vector3 screen2{};
      if (!ProjectWorldPointToScreen(world0, screen0) ||
          !ProjectWorldPointToScreen(world1, screen1) ||
          !ProjectWorldPointToScreen(world2, screen2))
      {
        continue;
      }

      if (debugPoints)
      {
        DrawPoint(static_cast<int>(screen0.x), static_cast<int>(screen0.y),
                  screen0.z, PackAARRGGBB(0xFFFFFFFF));
        DrawPoint(static_cast<int>(screen1.x), static_cast<int>(screen1.y),
                  screen1.z, PackAARRGGBB(0xFFFFFFFF));
        DrawPoint(static_cast<int>(screen2.x), static_cast<int>(screen2.y),
                  screen2.z, PackAARRGGBB(0xFFFFFFFF));
        continue;
      }

      const uint32_t fillColor = ShadeBunnyFace(world0, world1, world2);
      DrawTri(screen2, screen1, screen0, fillColor, true);
    }
  }

  // TODO: 10. Camera movement and interaction

private:
  int m_Width{0};
  int m_Height{0};
  uint32_t *m_Framebuffer{nullptr};
  WorldCamera *m_Camera{nullptr};

  Matrix4x4 m_ViewportMatrix, m_ProjectionMatrix, m_CameraMatrix;

  SDL_Renderer *m_Renderer{nullptr};
  SDL_Texture *m_MainTexture{nullptr};
  uint32_t m_FramebufferFormatEnum{SDL_PIXELFORMAT_ARGB8888};
  SDL_PixelFormat *m_FramebufferFormat{nullptr};
  TextureLoader *m_TextureLoader{nullptr};
  float *m_ZBuffer{nullptr};

  const float m_ZNear{0.1f}, m_ZFar{50.0f};

  float m_RotateRadian{0.0f};

  Mesh *m_SimpleCubeMesh{nullptr};
  std::vector<Vector3> m_SimpleCubeNormalizedVerts;
  Vector3 m_SimpleCubeCenter{0.0f, 0.0f, 0.0f};
  float m_SimpleCubeScale{1.0f};
  float m_SimpleCubeRotateRadian{0.0f};

  Mesh *m_BunnyMesh{nullptr};
  std::vector<Vector3> m_BunnyNormalizedVerts;
  Vector3 m_BunnyCenter{0.0f, 0.0f, 0.0f};
  float m_BunnyScale{1.0f};
};
