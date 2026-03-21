#pragma once

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Mesh.hpp"
#include "Shader.hpp"
#include "TextureLoader.hpp"
#include "MeshLoader.hpp"
#include "WorldCamera.hpp"

class Renderer {
 public:
  Renderer(SDL_Window* window, int width, int height)
      : m_Width(width), m_Height(height) {
    m_Framebuffer = new uint32_t[m_Width * m_Height];
    m_Camera = new WorldCamera();
    m_ZBuffer = new float[m_Width * m_Height];

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

    m_TextureLoader = new TextureLoader(*m_Renderer);
    m_MeshLoader = new MeshLoader(*m_Renderer);
    BuildBunnyMesh();
  }

  ~Renderer() {
    delete[] m_Framebuffer;
    delete[] m_ZBuffer;
    delete m_Camera;
    delete m_BunnyMesh;
    delete m_TextureLoader;
    delete m_MeshLoader;
    if (m_FramebufferFormat != nullptr) {
      SDL_FreeFormat(m_FramebufferFormat);
      m_FramebufferFormat = nullptr;
    }
    SDL_DestroyTexture(m_MainTexture);
    SDL_DestroyRenderer(m_Renderer);
  }

  void Render(double delta) {
    ClearBuffers();

    // RenderBunnyMesh(m_BasicLitShader, m_BasicLitShaderUniforms);
    //    RenderBunnyMesh(m_PhongShader, m_PhongShaderUniforms);
    RenderBunnyMesh(m_BlinnPhongShader, m_BlinnPhongShaderUniforms);

    SDL_UpdateTexture(m_MainTexture, nullptr, m_Framebuffer,
                      m_Width * static_cast<int>(sizeof(uint32_t)));
    SDL_RenderCopy(m_Renderer, m_MainTexture, nullptr, nullptr);
    SDL_RenderPresent(m_Renderer);

    SDL_Delay(32);
  }

  void HandleKeyInput(SDL_Keycode key) {
    switch (key) {
      case SDLK_UP: {
        m_Camera->fov++;
        m_ProjectionMatrix = Matrix4x4::identity;
        math::SetupPerspectiveProjectionMatrix(m_ProjectionMatrix,
                                               m_Camera->fov, m_Camera->aspect,
                                               m_ZNear, m_ZFar);
        break;
      }
      case SDLK_DOWN: {
        m_Camera->fov--;
        m_ProjectionMatrix = Matrix4x4::identity;
        math::SetupPerspectiveProjectionMatrix(m_ProjectionMatrix,
                                               m_Camera->fov, m_Camera->aspect,
                                               m_ZNear, m_ZFar);
        break;
      }
      case SDLK_RIGHT: {
        m_Camera->eye.x += 0.1f;
        m_CameraMatrix = Matrix4x4::identity;
        math::SetupCameraMatrix(m_CameraMatrix, m_Camera->eye, m_Camera->at,
                                m_Camera->up);
        break;
      }
      case SDLK_LEFT: {
        m_Camera->eye.x -= 0.1f;
        m_CameraMatrix = Matrix4x4::identity;
        math::SetupCameraMatrix(m_CameraMatrix, m_Camera->eye, m_Camera->at,
                                m_Camera->up);
        break;
      }
      case SDLK_r: {
        // Reset camera
        m_Camera->eye = {2.2f, 1.2f, -6.5f};
        m_Camera->at = {0.0f, 0.0f, 0.0f};
        m_Camera->up = {0.0f, 1.0f, 0.0f};
        m_Camera->fov = 45.0f;
        SetupMatrices();
        break;
      }
      default: {
        break;
      }
    }
  }

 private:
  void ClearBuffers() {
    std::fill(m_Framebuffer, m_Framebuffer + m_Width * m_Height, 0);
    std::fill(m_ZBuffer, m_ZBuffer + m_Width * m_Height, 1.0f);
  }

  uint32_t PackColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) const {
    if (m_FramebufferFormat == nullptr) {
      return (static_cast<uint32_t>(a) << 24) |
             (static_cast<uint32_t>(r) << 16) |
             (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    }
    return SDL_MapRGBA(m_FramebufferFormat, r, g, b, a);
  }

  void DrawPoint(int x, int y, float z, uint32_t color) {
    if (x >= m_Width || x < 0) return;
    if (y >= m_Height || y < 0) return;

    if (m_ZBuffer[x + y * m_Width] < z) {
      return;
    }

    m_Framebuffer[x + y * m_Width] = color;
    m_ZBuffer[x + y * m_Width] = z;
  }

  void TransformToScreen(Vector4& point) {
    point = m_ProjectionMatrix * (m_CameraMatrix * point);
    point.PerspectiveDivide();
    point = m_ViewportMatrix * point;
  }

  bool ProjectWorldPointToScreen(const Vector3& point, Vector3& out) {
    Vector4 clip = m_ProjectionMatrix *
                   (m_CameraMatrix * Vector4(point.x, point.y, point.z, 1.0f));
    return ProjectClipPointToScreen(clip, out);
  }

  bool ProjectClipPointToScreen(const Vector4& clipPosition,
                                Vector3& out) const {
    Vector4 clip = clipPosition;
    if (clip.w <= 1e-6f) {
      return false;
    }

    clip.PerspectiveDivide();
    if (!std::isfinite(clip.x) || !std::isfinite(clip.y) ||
        !std::isfinite(clip.z)) {
      return false;
    }
    if (clip.z < 0.0f || clip.z > 1.0f) {
      return false;
    }

    clip = m_ViewportMatrix * clip;
    out = {clip.x, clip.y, clip.z};
    return true;
  }

  void SetupMatrices() {
    math::SetupCameraMatrix(m_CameraMatrix, m_Camera->eye, m_Camera->at,
                            m_Camera->up);
    math::SetupPerspectiveProjectionMatrix(m_ProjectionMatrix, m_Camera->fov,
                                           m_Camera->aspect, m_ZNear, m_ZFar);
    math::SetupViewportMatrix(m_ViewportMatrix, 0, 0, m_Width, m_Height,
                              m_ZNear, m_ZFar);
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

  void DrawShaderTri(const VertexOutput& out0, const VertexOutput& out1,
                     const VertexOutput& out2, const ShaderUniforms& uniforms,
                     const Shader& shader, bool cullBackface = true) {
    Vector3 screen0{};
    Vector3 screen1{};
    Vector3 screen2{};
    if (!ProjectClipPointToScreen(out0.clipPosition, screen0) ||
        !ProjectClipPointToScreen(out1.clipPosition, screen1) ||
        !ProjectClipPointToScreen(out2.clipPosition, screen2)) {
      return;
    }

    const float minX = std::min({screen0.x, screen1.x, screen2.x});
    const float maxX = std::max({screen0.x, screen1.x, screen2.x});
    const float minY = std::min({screen0.y, screen1.y, screen2.y});
    const float maxY = std::max({screen0.y, screen1.y, screen2.y});

    const int x0 = std::max(0, static_cast<int>(std::floor(minX)));
    const int y0 = std::max(0, static_cast<int>(std::floor(minY)));
    const int x1 = std::min(m_Width - 1, static_cast<int>(std::ceil(maxX)));
    const int y1 = std::min(m_Height - 1, static_cast<int>(std::ceil(maxY)));

    const Vector2 p0{screen0.x, screen0.y};
    const Vector2 p1{screen1.x, screen1.y};
    const Vector2 p2{screen2.x, screen2.y};
    const float area = math::EdgeFunction(p0, p1, screen2.x, screen2.y);
    if (std::abs(area) < 1e-6f) {
      return;
    }

    if (cullBackface && area < 0.0f) {
      return;
    }

    const float invArea = 1.0f / area;

    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const float px = static_cast<float>(x) + 0.5f;
        const float py = static_cast<float>(y) + 0.5f;

        const float w0 = math::EdgeFunction(p1, p2, px, py);
        const float w1 = math::EdgeFunction(p2, p0, px, py);
        const float w2 = math::EdgeFunction(p0, p1, px, py);

        const bool insideCCW = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f);
        const bool insideCW = (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
        const bool inside = cullBackface ? insideCCW
                                         : ((area > 0.0f && insideCCW) ||
                                            (area < 0.0f && insideCW));
        if (!inside) {
          continue;
        }

        const float b0 = w0 * invArea;
        const float b1 = w1 * invArea;
        const float b2 = w2 * invArea;
        const float z = b0 * screen0.z + b1 * screen1.z + b2 * screen2.z;
        if (z < 0.0f || z > 1.0f) {
          continue;
        }

        const int idx = x + y * m_Width;
        if (z >= m_ZBuffer[idx]) {
          continue;
        }

        const float pw0 = b0 * out0.invW;
        const float pw1 = b1 * out1.invW;
        const float pw2 = b2 * out2.invW;
        const float invWeightSum = pw0 + pw1 + pw2;
        if (std::abs(invWeightSum) <= 1e-8f) {
          continue;
        }

        const float invSum = 1.0f / invWeightSum;
        PixelInput pixelInput{};
        pixelInput.worldPosition =
            (out0.worldPosition * pw0 + out1.worldPosition * pw1 +
             out2.worldPosition * pw2) *
            invSum;
        pixelInput.normal =
            (out0.normal * pw0 + out1.normal * pw1 + out2.normal * pw2) *
            invSum;
        pixelInput.uv =
            (out0.uv * pw0 + out1.uv * pw1 + out2.uv * pw2) * invSum;

        const Color color = shader.PixelShader(pixelInput, uniforms);
        m_ZBuffer[idx] = z;
        m_Framebuffer[idx] = PackColor(color.r, color.g, color.b, color.a);
      }
    }
  }

  void BuildBunnyMesh() {
     m_BunnyMesh = m_MeshLoader->LoadMesh("bunny.obj");
//    m_BunnyMesh = m_MeshLoader->LoadMesh("cube.obj");
    if (m_BunnyMesh != nullptr) {
      if (!m_BunnyMesh->verts.empty()) {
        Vector3 min = m_BunnyMesh->verts[0];
        Vector3 max = m_BunnyMesh->verts[0];
        for (const Vector3& v : m_BunnyMesh->verts) {
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
        if (maxExtent > 1e-6f) {
          m_BunnyScale = 4.0f / maxExtent;
        }
      }

      m_BunnyNormalizedVerts.resize(m_BunnyMesh->verts.size());
      for (size_t i = 0; i < m_BunnyMesh->verts.size(); ++i) {
        m_BunnyNormalizedVerts[i] =
            (m_BunnyMesh->verts[i] - m_BunnyCenter) * m_BunnyScale;
      }

      m_BunnyMesh->tga = m_TextureLoader->LoadTGATextureWithName("numbered_checker.tga");
    }
  }

  void RenderBunnyMesh(const Shader& shader, ShaderUniforms& uniforms) {
    if (m_BunnyMesh == nullptr) {
      return;
    }

    Matrix4x4 modelMat = Matrix4x4::identity;
    m_RotateRadian += 0.45f;
    if (m_RotateRadian > 360.0f) {
      m_RotateRadian -= 360.0f;
    }
    modelMat.Rotate(0.0f, m_RotateRadian, 0.0f);

    uniforms.model = modelMat;
    uniforms.view = m_CameraMatrix;
    uniforms.projection = m_ProjectionMatrix;
    uniforms.lightDir = m_LightDir.Normalize();
    uniforms.texture = m_BunnyMesh->tga;
    uniforms.cameraPosition = m_Camera->eye;

    // Fixed material components
    uniforms.ambientStrength = 0.25f;
    uniforms.diffuseStrength = 0.8f;
    uniforms.specularStrength = 1.4f;
    uniforms.shininess = 8.0f;
    //      uniforms.specularColor = Color(0xFF0000FF); // red color
    uniforms.specularColor = Color(0xFFFFFFFF);  // white color

    for (size_t idx = 0; idx + 2 < m_BunnyMesh->indices.size(); idx += 3) {
      uint32_t i0 = m_BunnyMesh->indices[idx];
      uint32_t i1 = m_BunnyMesh->indices[idx + 1];
      uint32_t i2 = m_BunnyMesh->indices[idx + 2];

      const VertexInput v0{m_BunnyNormalizedVerts[i0], m_BunnyMesh->normals[i0],
                           m_BunnyMesh->uvs[i0]};
      const VertexInput v1{m_BunnyNormalizedVerts[i1], m_BunnyMesh->normals[i1],
                           m_BunnyMesh->uvs[i1]};
      const VertexInput v2{m_BunnyNormalizedVerts[i2], m_BunnyMesh->normals[i2],
                           m_BunnyMesh->uvs[i2]};

      const VertexOutput out0 = shader.VertexShader(v0, uniforms);
      const VertexOutput out1 = shader.VertexShader(v1, uniforms);
      const VertexOutput out2 = shader.VertexShader(v2, uniforms);

      DrawShaderTri(out2, out1, out0, uniforms, shader, true);
    }
  }

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
  MeshLoader* m_MeshLoader{nullptr};
  float* m_ZBuffer{nullptr};

  const float m_ZNear{0.1f}, m_ZFar{50.0f};

  float m_RotateRadian{0.0f};

  Mesh* m_BunnyMesh{nullptr};
  std::vector<Vector3> m_BunnyNormalizedVerts;
  Vector3 m_BunnyCenter{0.0f, 0.0f, 0.0f};
  float m_BunnyScale{1.0f};

  Vector3 m_LightDir{-0.45f, 0.80f, -0.40f};

  //  // Using BasicLitShader
  //  BasicLitShader m_BasicLitShader;
  //  ShaderUniforms m_BasicLitShaderUniforms;
  //
  // Using PhongShader
  PhongShader m_PhongShader;
  ShaderUniforms m_PhongShaderUniforms;

  // Using BlinnPhongShader
  BlinnPhongShader m_BlinnPhongShader;
  ShaderUniforms m_BlinnPhongShaderUniforms;
};
