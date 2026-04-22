#include "Renderer.hpp"

Renderer::Renderer(SDL_Window *window, int width, int height)
    : IRenderer(window, width, height),
      m_Width(width),
      m_Height(height),
      m_ZBuffer(width, height),
      m_ShadowDepth(width, height)
{
  m_Framebuffer = new uint32_t[m_Width * m_Height];

  m_Renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (m_Renderer == nullptr)
  {
    LogF("SDL_CreateRenderer failed: %s", SDL_GetError());
    SDL_assert(false);
    return;
  }

  m_ResourceLoader = new ResourceLoader(*m_Renderer);
  m_FramebufferFormatEnum =
      m_ResourceLoader->GetSupportedTextureFormat(SDL_PIXELFORMAT_ARGB8888);

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

  m_Camera.aspect = static_cast<float>(m_Width) / m_Height;
  m_Camera.fov = 45.0f;

  SetupMatrices();
  LoadResources();
}

Renderer::~Renderer()
{
  delete[] m_Framebuffer;
  delete m_ResourceLoader;

  if (m_FramebufferFormat != nullptr)
  {
    SDL_FreeFormat(m_FramebufferFormat);
    m_FramebufferFormat = nullptr;
  }
  SDL_DestroyTexture(m_MainTexture);
  SDL_DestroyRenderer(m_Renderer);
}

void Renderer::Render(double delta)
{
  UpdateScene(delta);

  BeginFrame();

  RenderShadowPass();

  if (m_ShowShadowDepthOnly)
  {
    RenderDebugPass();
  }
  else
  {
    RenderMainPass();
  }

  EndFrame();
}

void Renderer::HandleKeyInput(const SDL_Event &event)
{
  switch (event.key.keysym.sym)
  {
  case SDLK_UP:
  {
    m_Camera.fov++;
    m_ProjectionMatrix = Matrix4x4::identity;
    math::SetupPerspectiveProjectionMatrix(m_ProjectionMatrix, m_Camera.fov,
                                           m_Camera.aspect, m_ZNear, m_ZFar);
    break;
  }
  case SDLK_DOWN:
  {
    m_Camera.fov--;
    m_ProjectionMatrix = Matrix4x4::identity;
    math::SetupPerspectiveProjectionMatrix(m_ProjectionMatrix, m_Camera.fov,
                                           m_Camera.aspect, m_ZNear, m_ZFar);
    break;
  }
  case SDLK_RIGHT:
  {
    m_Camera.eye.x += 0.1f;
    m_CameraMatrix = Matrix4x4::identity;
    math::SetupCameraMatrix(m_CameraMatrix, m_Camera.eye, m_Camera.at,
                            m_Camera.up);
    break;
  }
  case SDLK_LEFT:
  {
    m_Camera.eye.x -= 0.1f;
    m_CameraMatrix = Matrix4x4::identity;
    math::SetupCameraMatrix(m_CameraMatrix, m_Camera.eye, m_Camera.at,
                            m_Camera.up);
    break;
  }
  case SDLK_r:
  {
    m_Camera.eye = {2.2f, 1.2f, -6.5f};
    m_Camera.at = {0.0f, 0.0f, 0.0f};
    m_Camera.up = {0.0f, 1.0f, 0.0f};
    m_Camera.fov = 45.0f;
    SetupMatrices();
    break;
  }
  case SDLK_d:
  {
    const SDL_Keymod mods = static_cast<SDL_Keymod>(event.key.keysym.mod);
    const bool accelPressed =
        ((mods & KMOD_GUI) != 0) || ((mods & KMOD_CTRL) != 0);
    const bool shiftPressed = (mods & KMOD_SHIFT) != 0;
    if (accelPressed && shiftPressed)
    {
      m_ShowShadowDepthOnly = !m_ShowShadowDepthOnly;
      LogF("[ShadowDebug] depth visualization %s",
           m_ShowShadowDepthOnly ? "enabled" : "disabled");
    }
    break;
  }
  default:
  {
    break;
  }
  }
}

void Renderer::UpdateScene(double delta)
{
  (void)delta;

  if (m_Bunny == nullptr)
  {
    return;
  }

  m_RotateRadian += 0.6f;
  if (m_RotateRadian > 360.0f)
  {
    m_RotateRadian -= 360.0f;
  }

  m_Bunny->modelMatrix = Matrix4x4::identity;
  m_Bunny->modelMatrix.Rotate(0.0f, m_RotateRadian, 0.0f);
  m_Bunny->modelMatrix.Translate(m_BunnyTranslation.x, m_BunnyTranslation.y,
                                 m_BunnyTranslation.z);
}

void Renderer::BeginFrame() { ClearBuffers(); }

void Renderer::RenderShadowPass()
{
  RenderDepthMesh(*m_Plane, m_ShadowDepth);
  RenderDepthMesh(*m_Bunny, m_ShadowDepth);

  if (m_ShowShadowDepthOnly)
  {
    LogDepthTargetStats(m_ShadowDepth);
  }
}

void Renderer::RenderMainPass()
{
  DrawRenderable(*m_Plane);
  DrawRenderable(*m_Bunny);
}

void Renderer::RenderDebugPass() { VisualizeDepthTarget(m_ShadowDepth); }

void Renderer::EndFrame()
{
  SDL_UpdateTexture(m_MainTexture, nullptr, m_Framebuffer,
                    m_Width * static_cast<int>(sizeof(uint32_t)));
  SDL_RenderCopy(m_Renderer, m_MainTexture, nullptr, nullptr);
  SDL_RenderPresent(m_Renderer);

  SDL_Delay(16);
}

void Renderer::ClearBuffers()
{
  std::fill(m_Framebuffer, m_Framebuffer + m_Width * m_Height, 0);
  m_ZBuffer.Clear(1.0f);
  m_ShadowDepth.Clear(1.0f);
}

uint32_t Renderer::PackColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) const
{
  if (m_FramebufferFormat == nullptr)
  {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
  }
  return SDL_MapRGBA(m_FramebufferFormat, r, g, b, a);
}

bool Renderer::ProjectClipPointToScreen(const Vector4 &clipPosition,
                                        const Matrix4x4 &viewportMatrix,
                                        Vector3 &out) const
{
  Vector4 clip = clipPosition;
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

  clip = viewportMatrix * clip;
  out = {clip.x, clip.y, clip.z};
  return true;
}

void Renderer::SetupMatrices()
{
  math::SetupCameraMatrix(m_CameraMatrix, m_Camera.eye, m_Camera.at,
                          m_Camera.up);
  math::SetupPerspectiveProjectionMatrix(m_ProjectionMatrix, m_Camera.fov,
                                         m_Camera.aspect, m_ZNear, m_ZFar);
  math::SetupViewportMatrix(m_ViewportMatrix, 0, 0, m_Width, m_Height, 0.0f,
                            1.0f);

  SetupShadowMatrices();
  math::SetupViewportMatrix(m_ShadowViewportMatrix, 0, 0, m_ShadowDepth.width,
                            m_ShadowDepth.height, 0.0f, 1.0f);
}

void Renderer::SetupShadowMatrices()
{
  Vector3 lightUp{0.0f, 1.0f, 0.0f};
  const Vector3 lightForward = (m_LightTarget - m_LightPosition).Normalize();
  if (std::abs(math::DotProduct(lightForward, lightUp)) > 0.99f)
  {
    lightUp = {0.0f, 0.0f, 1.0f};
  }

  math::SetupCameraMatrix(m_LightViewMatrix, m_LightPosition, m_LightTarget,
                          lightUp);
  math::SetupOrthographicProjectionMatrix(
      m_LightProjectionMatrix, m_ShadowOrthoLeft, m_ShadowOrthoRight,
      m_ShadowOrthoBottom, m_ShadowOrthoTop, m_ShadowNear, m_ShadowFar);

  m_LightDir = (m_LightPosition - m_LightTarget).Normalize();
}

void Renderer::RasterizeTriangle(const VertexOutput &out0,
                                 const VertexOutput &out1,
                                 const VertexOutput &out2,
                                 const TriangleSetup &setup,
                                 const ShaderUniforms &uniforms,
                                 const Shader &shader, bool cullBackface)
{
  const Vector2 &p0 = {setup.v0.x, setup.v0.y};
  const Vector2 &p1 = {setup.v1.x, setup.v1.y};
  const Vector2 &p2 = {setup.v2.x, setup.v2.y};

  for (int y = setup.minY; y <= setup.maxY; ++y)
  {
    for (int x = setup.minX; x <= setup.maxX; ++x)
    {
      const float px = static_cast<float>(x) + 0.5f;
      const float py = static_cast<float>(y) + 0.5f;

      const float w0 = math::EdgeFunction(p1, p2, px, py);
      const float w1 = math::EdgeFunction(p2, p0, px, py);
      const float w2 = math::EdgeFunction(p0, p1, px, py);

      const bool insideCCW = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f);
      const bool insideCW = (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
      const bool inside = cullBackface ? insideCCW
                                       : ((setup.area > 0.0f && insideCCW) ||
                                          (setup.area < 0.0f && insideCW));
      if (!inside)
      {
        continue;
      }

      const float b0 = w0 * setup.invArea;
      const float b1 = w1 * setup.invArea;
      const float b2 = w2 * setup.invArea;
      const float z = b0 * setup.v0.z + b1 * setup.v1.z + b2 * setup.v2.z;
      if (z < 0.0f || z > 1.0f)
      {
        continue;
      }

      const int idx = x + y * m_Width;
      if (z >= m_ZBuffer.data[idx])
      {
        continue;
      }

      const float pw0 = b0 * out0.invW;
      const float pw1 = b1 * out1.invW;
      const float pw2 = b2 * out2.invW;
      const float invWeightSum = pw0 + pw1 + pw2;
      if (std::abs(invWeightSum) <= 1e-8f)
      {
        continue;
      }

      const float invSum = 1.0f / invWeightSum;
      PixelInput pixelInput{};
      pixelInput.worldPosition =
          (out0.worldPosition * pw0 + out1.worldPosition * pw1 +
           out2.worldPosition * pw2) *
          invSum;
      pixelInput.normal =
          (out0.normal * pw0 + out1.normal * pw1 + out2.normal * pw2) * invSum;
      pixelInput.uv = (out0.uv * pw0 + out1.uv * pw1 + out2.uv * pw2) * invSum;

      const Color color = shader.PixelShader(pixelInput, uniforms);
      m_ZBuffer.data[idx] = z;
      m_Framebuffer[idx] = PackColor(color.r, color.g, color.b, color.a);
    }
  }
}

void Renderer::RasterizeDepthTri(const Vector3 &v0, const Vector3 &v1,
                                 const Vector3 &v2,
                                 const Matrix4x4 &modelMatrix,
                                 render::DepthTarget &depthTarget)
{
  const Vector4 world0 = modelMatrix * Vector4(v0.x, v0.y, v0.z, 1.0f);
  const Vector4 world1 = modelMatrix * Vector4(v1.x, v1.y, v1.z, 1.0f);
  const Vector4 world2 = modelMatrix * Vector4(v2.x, v2.y, v2.z, 1.0f);

  const Vector4 clip0 = m_LightProjectionMatrix * (m_LightViewMatrix * world0);
  const Vector4 clip1 = m_LightProjectionMatrix * (m_LightViewMatrix * world1);
  const Vector4 clip2 = m_LightProjectionMatrix * (m_LightViewMatrix * world2);

  Vector3 screen0{};
  Vector3 screen1{};
  Vector3 screen2{};
  if (!ProjectClipPointToScreen(clip0, m_ShadowViewportMatrix, screen0) ||
      !ProjectClipPointToScreen(clip1, m_ShadowViewportMatrix, screen1) ||
      !ProjectClipPointToScreen(clip2, m_ShadowViewportMatrix, screen2))
  {
    return;
  }

  const float minX = std::min({screen0.x, screen1.x, screen2.x});
  const float maxX = std::max({screen0.x, screen1.x, screen2.x});
  const float minY = std::min({screen0.y, screen1.y, screen2.y});
  const float maxY = std::max({screen0.y, screen1.y, screen2.y});

  const int x0 = std::max(0, static_cast<int>(std::floor(minX)));
  const int y0 = std::max(0, static_cast<int>(std::floor(minY)));
  const int x1 =
      std::min(depthTarget.width - 1, static_cast<int>(std::ceil(maxX)));
  const int y1 =
      std::min(depthTarget.height - 1, static_cast<int>(std::ceil(maxY)));

  const Vector2 p0{screen0.x, screen0.y};
  const Vector2 p1{screen1.x, screen1.y};
  const Vector2 p2{screen2.x, screen2.y};
  const float area = math::EdgeFunction(p0, p1, p2.x, p2.y);
  if (std::abs(area) < 1e-6f || area < 0.0f)
  {
    return;
  }

  const float invArea = 1.0f / area;

  for (int y = y0; y <= y1; ++y)
  {
    for (int x = x0; x <= x1; ++x)
    {
      const float px = static_cast<float>(x) + 0.5f;
      const float py = static_cast<float>(y) + 0.5f;

      const float w0 = math::EdgeFunction(p1, p2, px, py);
      const float w1 = math::EdgeFunction(p2, p0, px, py);
      const float w2 = math::EdgeFunction(p0, p1, px, py);
      if (!(w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f))
      {
        continue;
      }

      const float b0 = w0 * invArea;
      const float b1 = w1 * invArea;
      const float b2 = w2 * invArea;
      const float z = b0 * screen0.z + b1 * screen1.z + b2 * screen2.z;
      if (z < 0.0f || z > 1.0f)
      {
        continue;
      }

      const int idx = x + y * depthTarget.width;
      if (z < depthTarget.data[idx])
      {
        depthTarget.data[idx] = z;
      }
    }
  }
}

void Renderer::RenderDepthMesh(const Renderable &renderable,
                               render::DepthTarget &depthTarget)
{
  if (renderable.mesh == nullptr)
  {
    return;
  }

  for (size_t idx = 0; idx + 2 < renderable.mesh->indices.size(); idx += 3)
  {
    const uint32_t i0 = renderable.mesh->indices[idx];
    const uint32_t i1 = renderable.mesh->indices[idx + 1];
    const uint32_t i2 = renderable.mesh->indices[idx + 2];
    RasterizeDepthTri(renderable.mesh->verts[i0], renderable.mesh->verts[i1],
                      renderable.mesh->verts[i2], renderable.modelMatrix,
                      depthTarget);
  }
}

void Renderer::LoadResources()
{
  if (m_ResourceLoader == nullptr)
  {
    return;
  }

  // TGA* checker =
  //     m_ResourceLoader->LoadTGATextureWithName("numbered_checker.tga");

  m_Bunny = m_ResourceLoader->LoadRenderable("bunny.obj");
  // m_Bunny->mesh->tga = checker;
  m_Bunny->shader = &m_Shader; // Blinn-Phong shader for main pass

  m_Plane = m_ResourceLoader->CreateGridPlaneMesh(12, 12, 12.0f, 12.0f);
  if (m_Plane->mesh != nullptr)
  {
    // m_Plane->mesh->tga = checker;
    const float bunnyBottomY =
        -1.968211f;              // bunny 모델의 바닥 Y 좌표 (최소 Y 값)
    const float epsilon = 0.01f; // 살짝 아래로 내려 z-fighting 방지

    m_Plane->modelMatrix = Matrix4x4::identity;
    m_Plane->modelMatrix.Translate(0.0f, bunnyBottomY - epsilon, 0.0f);
  }
  m_Plane->shader = &m_Shader; // Blinn-Phong shader for main pass

  // m_Material = Material::Gold();
  m_Material = Material::Iron();

  m_DirectionalLightPosition = Vector3(5.0f, 10.0f, 5.0f);
}

void Renderer::DrawRenderable(const Renderable &renderable)
{
  BuildUniforms(renderable, m_Uniforms);
  DrawMesh(*renderable.mesh, renderable.modelMatrix, renderable.shader,
           m_Uniforms);
}

void Renderer::BuildUniforms(const Renderable &renderable,
                             ShaderUniforms &uniforms) const
{
  uniforms.model = renderable.modelMatrix;
  uniforms.view = m_CameraMatrix;
  uniforms.projection = m_ProjectionMatrix;
  uniforms.lightView = m_LightViewMatrix;
  uniforms.lightProjection = m_LightProjectionMatrix;
  uniforms.shadowViewport = m_ShadowViewportMatrix;
  uniforms.shadowMap = &m_ShadowDepth;
  uniforms.lightDir = m_LightDir.Normalize();
  uniforms.cameraPosition = m_Camera.eye;
  uniforms.shadowBias = m_ShadowBias;

  uniforms.ambientStrength = 0.25f;
  uniforms.diffuseStrength = 0.8f;
  uniforms.specularStrength = 1.4f;
  uniforms.shininess = 8.0f;
  uniforms.specularColor = Color(0XFFFFFFFF); // white color

  if (renderable.mesh != nullptr)
  {
    uniforms.texture = renderable.mesh->tga;
  }

  PBRShaderUniforms *pbrUniforms = static_cast<PBRShaderUniforms *>(&uniforms);

  pbrUniforms->material = &m_Material;
  pbrUniforms->lightColor = Color(0XFFFFFFFF); // white color
  pbrUniforms->directionalLight = m_DirectionalLightPosition;
}

void Renderer::DrawMesh(const Mesh &mesh, const Matrix4x4 &modelMatrix,
                        const Shader *shader, const ShaderUniforms &uniforms)
{
  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
  {
    ProcessTriangle(mesh, i, modelMatrix, shader, uniforms);
  }
}

bool Renderer::BuildTriangleSetup(const VertexOutput &out0,
                                  const VertexOutput &out1,
                                  const VertexOutput &out2,
                                  TriangleSetup &setup) const
{
  // Project clip space positions to screen space
  Vector3 screen0{};
  Vector3 screen1{};
  Vector3 screen2{};
  if (!ProjectClipPointToScreen(out0.clipPosition, m_ViewportMatrix, screen0) ||
      !ProjectClipPointToScreen(out1.clipPosition, m_ViewportMatrix, screen1) ||
      !ProjectClipPointToScreen(out2.clipPosition, m_ViewportMatrix, screen2))
  {
    return false;
  }

  setup.v0 = {screen0.x, screen0.y, screen0.z};
  setup.v1 = {screen1.x, screen1.y, screen1.z};
  setup.v2 = {screen2.x, screen2.y, screen2.z};

  setup.area =
      math::EdgeFunction({setup.v0.x, setup.v0.y}, {setup.v1.x, setup.v1.y},
                         setup.v2.x, setup.v2.y);

  if (std::abs(setup.area) < 1e-6f)
  {
    return false;
  }

  setup.invArea = 1.0f / setup.area;

  // Compute bounding box of the triangle in screen space
  const float minX = std::min({setup.v0.x, setup.v1.x, setup.v2.x});
  const float maxX = std::max({setup.v0.x, setup.v1.x, setup.v2.x});
  const float minY = std::min({setup.v0.y, setup.v1.y, setup.v2.y});
  const float maxY = std::max({setup.v0.y, setup.v1.y, setup.v2.y});

  setup.minX = std::max(0, static_cast<int>(std::floor(minX)));
  setup.minY = std::max(0, static_cast<int>(std::floor(minY)));
  setup.maxX = std::min(m_Width - 1, static_cast<int>(std::ceil(maxX)));
  setup.maxY = std::min(m_Height - 1, static_cast<int>(std::ceil(maxY)));

  if (setup.minX > setup.maxX || setup.minY > setup.maxY)
  {
    return false;
  }

  return true;
}

void Renderer::ProcessTriangle(const Mesh &mesh, size_t index,
                               const Matrix4x4 &modelMatrix,
                               const Shader *shader,
                               const ShaderUniforms &uniforms)
{
  const uint32_t i0 = mesh.indices[index];
  const uint32_t i1 = mesh.indices[index + 1];
  const uint32_t i2 = mesh.indices[index + 2];

  const VertexInput v0{mesh.verts[i0], mesh.normals[i0], mesh.uvs[i0]};
  const VertexInput v1{mesh.verts[i1], mesh.normals[i1], mesh.uvs[i1]};
  const VertexInput v2{mesh.verts[i2], mesh.normals[i2], mesh.uvs[i2]};

  const VertexOutput out0 = shader->VertexShader(v0, uniforms);
  const VertexOutput out1 = shader->VertexShader(v1, uniforms);
  const VertexOutput out2 = shader->VertexShader(v2, uniforms);

  TriangleSetup setup;
  if (not BuildTriangleSetup(out2, out1, out0, setup))
  {
    return;
  }

  if (setup.area < 0.0f)
  {
    return;
  }

  RasterizeTriangle(out2, out1, out0, setup, uniforms, *shader, true);
}

void Renderer::VisualizeDepthTarget(const render::DepthTarget &depthTarget)
{
  float minDepth = 1.0f;
  float maxDepth = 0.0f;
  bool hasWrittenDepth = false;

  for (float depthValue : depthTarget.data)
  {
    if (depthValue >= 1.0f)
    {
      continue;
    }
    minDepth = std::min(minDepth, depthValue);
    maxDepth = std::max(maxDepth, depthValue);
    hasWrittenDepth = true;
  }

  for (int y = 0; y < m_Height; ++y)
  {
    const int sy =
        std::min(depthTarget.height - 1, y * depthTarget.height / m_Height);
    for (int x = 0; x < m_Width; ++x)
    {
      const int sx =
          std::min(depthTarget.width - 1, x * depthTarget.width / m_Width);
      const int depthIdx = sx + sy * depthTarget.width;
      const int framebufferIdx = x + y * m_Width;

      const float depthValue = depthTarget.data[depthIdx];
      uint8_t intensity = 0;
      if (hasWrittenDepth && depthValue < 1.0f)
      {
        float normalizedDepth = 0.0f;
        if (maxDepth > minDepth + 1e-6f)
        {
          normalizedDepth = (depthValue - minDepth) / (maxDepth - minDepth);
        }
        intensity = static_cast<uint8_t>((1.0f - normalizedDepth) * 255.0f);
      }

      m_Framebuffer[framebufferIdx] =
          PackColor(intensity, intensity, intensity, 255);
    }
  }
}

void Renderer::LogDepthTargetStats(
    const render::DepthTarget &depthTarget) const
{
  if (depthTarget.data.empty())
  {
    LogF("[ShadowDebug] depth target is empty");
    return;
  }

  float minDepth = 1.0f;
  float maxDepth = 0.0f;
  int writtenPixels = 0;
  for (float depthValue : depthTarget.data)
  {
    if (depthValue < 1.0f)
    {
      minDepth = std::min(minDepth, depthValue);
      maxDepth = std::max(maxDepth, depthValue);
      ++writtenPixels;
    }
  }

  if (writtenPixels == 0)
  {
    LogF("[ShadowDebug] written=0 size=%dx%d", depthTarget.width,
         depthTarget.height);
    return;
  }

  LogF("[ShadowDebug] written=%d size=%dx%d min=%.6f max=%.6f", writtenPixels,
       depthTarget.width, depthTarget.height, minDepth, maxDepth);
}
