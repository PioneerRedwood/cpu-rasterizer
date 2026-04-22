#include "GLRenderer.hpp"
#include "Log.hpp"

#include <cstddef>
#include <filesystem>

namespace {
constexpr float kCameraMoveStep = 0.35f;
constexpr float kCameraYawStepDegrees = 10.0f;

constexpr const char* s_vsh = R"(#version 150

in vec3 position;
in vec3 normal;
in vec2 uv;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec2 vUv;

void main()
{
  vec4 worldPosition = uModel * vec4(position, 1.0);
  vWorldPosition = worldPosition.xyz;
  vWorldNormal = normalize(mat3(uModel) * normal);
  vUv = uv;
  gl_Position = uProjection * uView * worldPosition;
}
)";

constexpr const char* s_fsh = R"(#version 150

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec2 vUv;

uniform vec3 uLightDir;
uniform vec3 uCameraPosition;
uniform float uAmbientStrength;
uniform float uDiffuseStrength;
uniform float uSpecularStrength;
uniform float uShininess;
uniform vec3 uSpecularColor;
uniform vec3 uAlbedo;
uniform int uUseTexture;
uniform sampler2D uDiffuseTexture;

out vec4 fragColor;

void main()
{
  vec3 n = normalize(vWorldNormal);
  vec3 l = normalize(uLightDir);
  vec3 v = normalize(uCameraPosition - vWorldPosition);
  vec3 h = normalize(l + v);
  float ndotl = max(dot(n, l), 0.0);
  float diffuse = uDiffuseStrength * ndotl;
  float specular = ndotl > 0.0 ? uSpecularStrength * pow(max(dot(n, h), 0.0), uShininess) : 0.0;
  vec3 baseColor = uUseTexture != 0 ? texture(uDiffuseTexture, vUv).rgb : uAlbedo;
  vec3 color = baseColor * uAmbientStrength + (baseColor * diffuse + uSpecularColor * specular);
  fragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
)";

struct GLVertex
{
  Vector3 position;
  Vector3 normal;
  Vector2 uv;
};

bool CheckShaderCompile(GLuint shader, const char* label)
{
  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_TRUE)
  {
    return true;
  }

  GLint logLength = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
  char log[1024] = {};
  if (logLength > 0)
  {
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
  }

  LogF("[GL] %s compile failed: %s", label, logLength > 0 ? log : "unknown error");
  return false;
}

bool CheckProgramLink(GLuint program, const char* label)
{
  GLint linked = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked == GL_TRUE)
  {
    return true;
  }

  GLint logLength = 0;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
  char log[1024] = {};
  if (logLength > 0)
  {
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
  }

  LogF("[GL] %s link failed: %s", label, logLength > 0 ? log : "unknown error");
  return false;
}

Vector3 RotateYaw(const Vector3& v, float degrees)
{
  const float radians = degrees * static_cast<float>(acos(-1.0) / 180.0);
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {
      c * v.x + s * v.z,
      v.y,
      -s * v.x + c * v.z,
  };
}

}

GLRenderer::GLRenderer(SDL_Window* window, int width, int height) 
: IRenderer(window, width, height),
  m_Window(window),
  m_Width(width),
  m_Height(height)
{
  m_Camera.aspect = (m_Height != 0) ? static_cast<float>(m_Width) / static_cast<float>(m_Height) : 1.0f;
  m_Camera.fov = 60.0f;
  // GL path uses a conventional right-handed camera looking toward the origin.
  m_Camera.eye.z = -m_Camera.eye.z;
  LoadResources();
}

GLRenderer::~GLRenderer()
{
  auto DestroyGeometry = [](GLRenderer::GeometryBuffer& geometry)
  {
    if (geometry.ebo != 0)
    {
      glDeleteBuffers(1, &geometry.ebo);
    }
    if (geometry.vbo != 0)
    {
      glDeleteBuffers(1, &geometry.vbo);
    }
    if (geometry.vao != 0)
    {
      glDeleteVertexArrays(1, &geometry.vao);
    }
  };

  if (m_RenderCubeProgram != 0)
  {
    glDeleteProgram(m_RenderCubeProgram);
  }
  if (m_CheckerTexture != 0)
  {
    glDeleteTextures(1, &m_CheckerTexture);
  }
  if (m_VertexShader != 0)
  {
    glDeleteShader(m_VertexShader);
  }
  if (m_FragmentShader != 0)
  {
    glDeleteShader(m_FragmentShader);
  }

  DestroyGeometry(m_CubeGeometry);
  DestroyGeometry(m_BunnyGeometry);
}

void GLRenderer::HandleKeyInput(const SDL_Event& event) 
{
  const SDL_Scancode key = event.key.keysym.scancode;
  const Vector3 forward = (m_Camera.at - m_Camera.eye).Normalize();
  const Vector3 right = math::CrossProduct(forward, m_Camera.up).Normalize();

  switch (key)
  {
    case SDL_SCANCODE_W:
    {
      const Vector3 delta = forward * kCameraMoveStep;
      m_Camera.eye = m_Camera.eye + delta;
      m_Camera.at = m_Camera.at + delta;
      break;
    }
    case SDL_SCANCODE_S:
    {
      const Vector3 delta = forward * kCameraMoveStep;
      m_Camera.eye = m_Camera.eye - delta;
      m_Camera.at = m_Camera.at - delta;
      break;
    }
    case SDL_SCANCODE_A:
    {
      const Vector3 delta = right * kCameraMoveStep;
      m_Camera.eye = m_Camera.eye - delta;
      m_Camera.at = m_Camera.at - delta;
      break;
    }
    case SDL_SCANCODE_D:
    {
      const Vector3 delta = right * kCameraMoveStep;
      m_Camera.eye = m_Camera.eye + delta;
      m_Camera.at = m_Camera.at + delta;
      break;
    }
    case SDL_SCANCODE_Q:
    {
      const Vector3 rotatedForward = RotateYaw(forward, kCameraYawStepDegrees);
      m_Camera.at = m_Camera.eye + rotatedForward;
      break;
    }
    case SDL_SCANCODE_E:
    {
      const Vector3 rotatedForward = RotateYaw(forward, -kCameraYawStepDegrees);
      m_Camera.at = m_Camera.eye + rotatedForward;
      break;
    }
    default:
    {
      break;
    }
  }
}

void GLRenderer::Render(double delta)
{
  const float deltaMilliseconds = static_cast<float>(std::clamp(delta, 0.0, 33.0));
  const float deltaSeconds = deltaMilliseconds * 0.001f;
  m_ModelRotationYDegrees += 45.0f * deltaSeconds;
  if (m_ModelRotationYDegrees >= 360.0f)
  {
    m_ModelRotationYDegrees -= 360.0f;
  }

  UpdateScenes();
  SetupMatrices();
  BeginFrame();
  RenderMainPass();

  EndFrame();
}

void GLRenderer::BeginFrame()
{
  glViewport(0, 0, m_Width, m_Height);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLRenderer::EndFrame()
{
  SDL_GL_SwapWindow(m_Window);
}

void GLRenderer::RenderMainPass()
{
  if (m_RenderCubeProgram == 0 || m_ModelLocation < 0 || m_ViewLocation < 0 ||
      m_ProjectionLocation < 0)
  {
    return;
  }

  glUseProgram(m_RenderCubeProgram);
  glUniformMatrix4fv(m_ViewLocation, 1, GL_FALSE, m_ViewMatrix.Data());
  glUniformMatrix4fv(m_ProjectionLocation, 1, GL_FALSE,
                     m_ProjectionMatrix.Data());
  glUniform3f(m_LightDirLocation, -0.45f, 0.8f, 0.35f);
  glUniform3f(m_CameraPositionLocation, m_Camera.eye.x, m_Camera.eye.y,
              m_Camera.eye.z);
  glUniform1f(m_AmbientStrengthLocation, 0.25f);
  glUniform1f(m_DiffuseStrengthLocation, 0.8f);
  glUniform1f(m_SpecularStrengthLocation, 1.4f);
  glUniform1f(m_ShininessLocation, 8.0f);
  glUniform3f(m_SpecularColorLocation, 1.0f, 1.0f, 1.0f);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_CheckerTexture);
  glUniform1i(m_DiffuseTextureLocation, 0);

  DrawGeometry(m_CubeGeometry, m_PlaneModelMatrix, {0.42f, 0.44f, 0.46f}, true);
  DrawGeometry(m_BunnyGeometry, m_BunnyModelMatrix, {0.78f, 0.78f, 0.82f}, false);

  glUseProgram(0);
}

void GLRenderer::RenderDebugPass()
{

}

void GLRenderer::LoadResources()
{
  if (!CreateAndCompileShaders())
  {
    return;
  }

  if (!LinkProgram())
  {
    return;
  }

  if (!CreateGeometryBuffers(m_CubeGeometry) ||
      !CreateGeometryBuffers(m_BunnyGeometry))
  {
    return;
  }

  if (!LoadSceneMeshes())
  {
    return;
  }
  if (!LoadCheckerTexture())
  {
    return;
  }
  UpdateScenes();
  SetupMatrices();
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
}

void GLRenderer::SetupMatrices()
{
  m_ViewMatrix = glmath::LookAtRH(m_Camera.eye, m_Camera.at, m_Camera.up);
  m_ProjectionMatrix =
      glmath::PerspectiveRH(m_Camera.fov, m_Camera.aspect, 0.1f, 100.0f);
}

void GLRenderer::UpdateScenes()
{
  const glmath::Mat4 rotation = glmath::RotationY(m_ModelRotationYDegrees);
  m_PlaneModelMatrix = glmath::Translation(0.0f, -2.95f, -6.0f) *
                       glmath::Scale(10.0f, 0.08f, 10.0f);
  m_BunnyModelMatrix = glmath::Translation(0.0f, -0.6f, -6.0f) *
                       glmath::Scale(1.25f, 1.25f, 1.25f) * rotation;
}

bool GLRenderer::CreateAndCompileShaders()
{
  m_VertexShader = glCreateShader(GL_VERTEX_SHADER);
  const GLchar* vshSrc = s_vsh;
  glShaderSource(m_VertexShader, 1, &vshSrc, nullptr);
  glCompileShader(m_VertexShader);
  if (!CheckShaderCompile(m_VertexShader, "Vertex shader"))
  {
    return false;
  }

  m_FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  const GLchar* fshSrc = s_fsh;
  glShaderSource(m_FragmentShader, 1, &fshSrc, nullptr);
  glCompileShader(m_FragmentShader);
  if (!CheckShaderCompile(m_FragmentShader, "Fragment shader"))
  {
    return false;
  }

  return true;
}

bool GLRenderer::CreateGeometryBuffers(GeometryBuffer& geometry)
{
  glGenVertexArrays(1, &geometry.vao);
  glGenBuffers(1, &geometry.vbo);
  glGenBuffers(1, &geometry.ebo);

  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    LogF("[GL] Failed to create geometry buffers: %u", err);
    return false;
  }

  return true;
}

bool GLRenderer::LinkProgram()
{
  m_RenderCubeProgram = glCreateProgram();
  glAttachShader(m_RenderCubeProgram, m_VertexShader);
  glAttachShader(m_RenderCubeProgram, m_FragmentShader);
  glBindAttribLocation(m_RenderCubeProgram, 0, "position");
  glBindAttribLocation(m_RenderCubeProgram, 1, "normal");
  glBindAttribLocation(m_RenderCubeProgram, 2, "uv");
  glLinkProgram(m_RenderCubeProgram);

  if (!CheckProgramLink(m_RenderCubeProgram, "RenderCubeProgram"))
  {
    return false;
  }

  m_ModelLocation = glGetUniformLocation(m_RenderCubeProgram, "uModel");
  m_ViewLocation = glGetUniformLocation(m_RenderCubeProgram, "uView");
  m_ProjectionLocation = glGetUniformLocation(m_RenderCubeProgram, "uProjection");
  m_LightDirLocation = glGetUniformLocation(m_RenderCubeProgram, "uLightDir");
  m_CameraPositionLocation =
      glGetUniformLocation(m_RenderCubeProgram, "uCameraPosition");
  m_AmbientStrengthLocation =
      glGetUniformLocation(m_RenderCubeProgram, "uAmbientStrength");
  m_DiffuseStrengthLocation =
      glGetUniformLocation(m_RenderCubeProgram, "uDiffuseStrength");
  m_SpecularStrengthLocation =
      glGetUniformLocation(m_RenderCubeProgram, "uSpecularStrength");
  m_ShininessLocation = glGetUniformLocation(m_RenderCubeProgram, "uShininess");
  m_SpecularColorLocation =
      glGetUniformLocation(m_RenderCubeProgram, "uSpecularColor");
  m_AlbedoLocation = glGetUniformLocation(m_RenderCubeProgram, "uAlbedo");
  m_UseTextureLocation = glGetUniformLocation(m_RenderCubeProgram, "uUseTexture");
  m_DiffuseTextureLocation =
      glGetUniformLocation(m_RenderCubeProgram, "uDiffuseTexture");
  if (m_ModelLocation < 0 || m_ViewLocation < 0 || m_ProjectionLocation < 0 ||
      m_LightDirLocation < 0 || m_CameraPositionLocation < 0 ||
      m_AmbientStrengthLocation < 0 || m_DiffuseStrengthLocation < 0 ||
      m_SpecularStrengthLocation < 0 || m_ShininessLocation < 0 ||
      m_SpecularColorLocation < 0 || m_AlbedoLocation < 0 ||
      m_UseTextureLocation < 0 || m_DiffuseTextureLocation < 0)
  {
    LogF("[GL] Failed to find one or more Blinn-Phong uniform locations");
    return false;
  }

  return true;
}

bool GLRenderer::UploadMeshToGeometry(const Mesh& mesh, GeometryBuffer& geometry)
{
  if (mesh.verts.empty() || mesh.indices.empty())
  {
    LogF("[GL] UploadMesh skipped: mesh has no vertices or indices");
    return false;
  }

  geometry.indexCount = static_cast<GLsizei>(mesh.indices.size());
  std::vector<GLVertex> vertices;
  vertices.reserve(mesh.verts.size());
  for (size_t i = 0; i < mesh.verts.size(); ++i)
  {
    const Vector3 normal =
        (i < mesh.normals.size()) ? mesh.normals[i] : Vector3(0.0f, 1.0f, 0.0f);
    const Vector2 uv = (i < mesh.uvs.size()) ? mesh.uvs[i] : Vector2(0.0f, 0.0f);
    vertices.push_back({mesh.verts[i], normal, uv});
  }

  glBindVertexArray(geometry.vao);

  glBindBuffer(GL_ARRAY_BUFFER, geometry.vbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(GLVertex)),
               vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint32_t)),
               mesh.indices.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                        reinterpret_cast<const void*>(offsetof(GLVertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                        reinterpret_cast<const void*>(offsetof(GLVertex, normal)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                        reinterpret_cast<const void*>(offsetof(GLVertex, uv)));

  glBindVertexArray(0);
  return true;
}

bool GLRenderer::LoadSceneMeshes()
{
  m_CubeMesh = ResourceLoader::LoadMeshStandalone("cube.obj");
  if (!m_CubeMesh)
  {
    LogF("[GL] Failed to load cube.obj");
    return false;
  }

  m_BunnyMesh = ResourceLoader::LoadMeshStandalone("bunny.obj");
  if (!m_BunnyMesh)
  {
    LogF("[GL] Failed to load bunny.obj");
    return false;
  }

  LogF("[GL] Loaded cube.obj with %zu vertices and %zu indices",
       m_CubeMesh->verts.size(), m_CubeMesh->indices.size());
  LogF("[GL] Loaded bunny.obj with %zu vertices and %zu indices",
       m_BunnyMesh->verts.size(), m_BunnyMesh->indices.size());

  return UploadMeshToGeometry(*m_CubeMesh, m_CubeGeometry) &&
         UploadMeshToGeometry(*m_BunnyMesh, m_BunnyGeometry);
}

void GLRenderer::DrawGeometry(const GeometryBuffer& geometry,
                              const glmath::Mat4& modelMatrix,
                              const Vector3& albedo,
                              bool useTexture)
{
  if (geometry.vao == 0 || geometry.indexCount == 0)
  {
    return;
  }

  glUniformMatrix4fv(m_ModelLocation, 1, GL_FALSE, modelMatrix.Data());
  glUniform3f(m_AlbedoLocation, albedo.x, albedo.y, albedo.z);
  glUniform1i(m_UseTextureLocation, useTexture ? 1 : 0);
  glBindVertexArray(geometry.vao);
  glDrawElements(GL_TRIANGLES, geometry.indexCount, GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(0);
}

bool GLRenderer::LoadCheckerTexture()
{
  namespace fs = std::filesystem;
  const fs::path texturePath =
      fs::path(CPURASTERIZER_RESOURCE_DIR) / "numbered_checker.tga";

  TGA texture;
  if (!texture.ReadFromFile(texturePath.string().c_str(), SDL_PIXELFORMAT_RGBA32))
  {
    LogF("[GL] Failed to load checker texture from %s", texturePath.string().c_str());
    return false;
  }

  if (m_CheckerTexture == 0)
  {
    glGenTextures(1, &m_CheckerTexture);
  }

  glBindTexture(GL_TEXTURE_2D, m_CheckerTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture.Header()->width,
               texture.Header()->height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               texture.PixelData());
  glBindTexture(GL_TEXTURE_2D, 0);

  GLenum err = glGetError();
  if (err != GL_NO_ERROR)
  {
    LogF("[GL] Failed to upload checker texture: %u", err);
    return false;
  }

  LogF("[GL] Loaded checker texture %dx%d", texture.Header()->width,
       texture.Header()->height);
  return true;
}
