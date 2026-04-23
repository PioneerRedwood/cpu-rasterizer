#pragma once 

#include "GLMath.hpp"
#include "IRenderer.hpp"
#include "Material.hpp"
#include "Mesh.hpp"
#include "ResourceLoader.hpp"
#include "TGA.hpp"
#include "WorldCamera.hpp"

#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#include <memory>

class GLRenderer : public IRenderer 
{
public:
  GLRenderer(SDL_Window* window, int width, int height);

  ~GLRenderer();

  void Render(double delta) override;

  void HandleKeyInput(const SDL_Event& event) override;

private:
  void BeginFrame();

  void EndFrame();

  void RenderMainPass();

  void RenderDebugPass();

private:
  void SetupMatrices();

  void SetupShadowMatrices();

  void ClearBuffers();

  void LoadResources();

  void UpdateScenes();

  struct GeometryBuffer
  {
    GLuint vao { 0 };
    GLuint vbo { 0 };
    GLuint ebo { 0 };
    GLsizei indexCount { 0 };
  };

  struct GLMaterial
  {
    Material material { Material::RoughPlastic() };
    bool useTexture { false };
    GLuint diffuseTexture { 0 };
  };

private:
  bool CreateAndCompileShaders();

  bool CreateGeometryBuffers(GeometryBuffer& geometry);

  bool LinkProgram();

  bool UploadMeshToGeometry(const Mesh& mesh, GeometryBuffer& geometry);

  bool LoadSceneMeshes();

  bool LoadCheckerTexture();

  void SetupMaterials();

  void ApplyMaterial(const GLMaterial& material);

  void DrawGeometry(const GeometryBuffer& geometry,
                    const glmath::Mat4& modelMatrix,
                    const GLMaterial& material);

private:
  SDL_Window* m_Window { nullptr };
  int m_Width { 0 };
  int m_Height { 0 };

  WorldCamera m_Camera;
  glmath::Mat4 m_ViewMatrix { glmath::Identity() };
  glmath::Mat4 m_ProjectionMatrix { glmath::Identity() };
  glmath::Mat4 m_PlaneModelMatrix { glmath::Identity() };
  glmath::Mat4 m_BunnyModelMatrix { glmath::Identity() };
  float m_ModelRotationYDegrees { 0.0f };
  std::unique_ptr<Mesh> m_CubeMesh;
  std::unique_ptr<Mesh> m_BunnyMesh;
  GLMaterial m_PlaneMaterial;
  GLMaterial m_BunnyMaterial;

  GeometryBuffer m_CubeGeometry;
  GeometryBuffer m_BunnyGeometry;
  GLuint m_VertexShader { 0 };
  GLuint m_FragmentShader { 0 };
  GLuint m_RenderCubeProgram { 0 };
  GLuint m_CheckerTexture { 0 };
  GLint m_ModelLocation { -1 };
  GLint m_ViewLocation { -1 };
  GLint m_ProjectionLocation { -1 };
  GLint m_LightPositionLocation { -1 };
  GLint m_LightColorLocation { -1 };
  GLint m_CameraPositionLocation { -1 };
  GLint m_AlbedoLocation { -1 };
  GLint m_MetallicLocation { -1 };
  GLint m_RoughnessLocation { -1 };
  GLint m_AmbientOcclusionLocation { -1 };
  GLint m_UseTextureLocation { -1 };
  GLint m_DiffuseTextureLocation { -1 };

  // Shaders, Matrices
  // Renderable objects (bunny, plane or others)
  // 
};
