#pragma once

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "IRenderer.hpp"
#include "Material.hpp"
#include "ResourceLoader.hpp"
#include "WorldCamera.hpp"
#include "render/DepthTarget.hpp"
#include "TriangleSetup.hpp"

class Renderer : public IRenderer
{
public:
  Renderer(SDL_Window *window, int width, int height);

  ~Renderer();

  void Render(double delta) override;

  void HandleKeyInput(const SDL_Event &event) override;

private:
  void BeginFrame();

  void EndFrame();

  void RenderShadowPass();

  void RenderMainPass();

  void RenderDebugPass();

private:
  void SetupMatrices();

  void SetupShadowMatrices();

  void ClearBuffers();

  uint32_t PackColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) const;

  void LoadResources();

  void UpdateScene(double delta);

  bool ProjectClipPointToScreen(const Vector4 &clipPosition,
                                const Matrix4x4 &viewportMatrix,
                                Vector3 &out) const;

private:
  void RasterizeTriangle(const VertexOutput &out0, const VertexOutput &out1,
                         const VertexOutput &out2, const TriangleSetup &setup, const ShaderUniforms &uniforms,
                         const Shader &shader, bool cullBackface = true);

  void RasterizeDepthTri(const Vector3 &v0, const Vector3 &v1,
                         const Vector3 &v2, const Matrix4x4 &modelMatrix,
                         render::DepthTarget &depthTarget);

  void VisualizeDepthTarget(const render::DepthTarget &depthTarget);

  void LogDepthTargetStats(const render::DepthTarget &depthTarget) const;

  void RenderDepthMesh(const Renderable &model, render::DepthTarget &depthTarget);

  void DrawRenderable(const Renderable &renderable);

  void BuildUniforms(const Renderable &renderable, ShaderUniforms &uniforms) const;

  void DrawMesh(const Mesh &mesh, const Matrix4x4 &modelMatrix,
                const Shader *shader, const ShaderUniforms &uniforms);

  bool BuildTriangleSetup(const VertexOutput &out0, const VertexOutput &out1, const VertexOutput &out2,
                          TriangleSetup &triangleSetup) const;

  void ProcessTriangle(const Mesh &mesh, size_t index, const Matrix4x4 &modelMatrix,
                       const Shader *shader, const ShaderUniforms &uniforms);

private:
  int m_Width{0};
  int m_Height{0};
  uint32_t *m_Framebuffer{nullptr};

  SDL_Renderer *m_Renderer{nullptr};
  SDL_Texture *m_MainTexture{nullptr};
  uint32_t m_FramebufferFormatEnum{SDL_PIXELFORMAT_ARGB8888};
  SDL_PixelFormat *m_FramebufferFormat{nullptr};

  WorldCamera m_Camera;
  Matrix4x4 m_ViewportMatrix;
  Matrix4x4 m_ProjectionMatrix;
  Matrix4x4 m_CameraMatrix;

  const float m_ZNear{1.0f};
  const float m_ZFar{200.0f};

  ResourceLoader *m_ResourceLoader{nullptr};
  render::DepthTarget m_ZBuffer;

  float m_RotateRadian{0.0f};
  Vector3 m_BunnyTranslation{0.0f, 0.0f, 0.0f};

  Renderable *m_Bunny{nullptr};
  Renderable *m_Plane{nullptr};
#if 0
  BlinnPhongShader m_Shader;
  ShaderUniforms m_Uniforms;
#else
  PBRShader m_Shader;
  PBRShaderUniforms m_Uniforms;
  Material m_Material;
  Vector3 m_DirectionalLightPosition;
#endif

private:
  // Depth map and shadow settings
  Vector3 m_LightPosition{-4.0f, 6.0f, -3.0f};
  Vector3 m_LightTarget{0.0f, 0.0f, 0.0f};
  Vector3 m_LightDir{-0.51f, 0.77f, -0.38f};

  float m_ShadowOrthoLeft{-8.0f};
  float m_ShadowOrthoRight{8.0f};
  float m_ShadowOrthoBottom{-6.0f};
  float m_ShadowOrthoTop{6.0f};
  float m_ShadowNear{0.1f};
  float m_ShadowFar{20.0f};

  Matrix4x4 m_LightViewMatrix;
  Matrix4x4 m_LightProjectionMatrix;
  Matrix4x4 m_ShadowViewportMatrix;

  render::DepthTarget m_ShadowDepth;

  float m_ShadowBias{0.003f};

private:
  // DEBUG
  bool m_ShowShadowDepthOnly{false};
};
