#pragma once 

#include "IRenderer.hpp"
#include "WorldCamera.hpp"

#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

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

private:
  bool CreateAndCompileShaders();

  bool CreateGPUBuffers();

  bool LinkProgram();

  void BuildCube();

private:
  SDL_Window* m_Window { nullptr };
  int m_Width { 0 };
  int m_Height { 0 };

  WorldCamera m_Camera;

  GLuint m_Vao { 0 };
  GLuint m_Vbo { 0 };
  GLuint m_Ebo { 0 };
  GLuint m_VertexShader { 0 };
  GLuint m_FragmentShader { 0 };
  GLuint m_RenderCubeProgram { 0 };
  GLsizei m_CubeIndexCount { 0 };

  // Shaders, Matrices
  // Renderable objects (bunny, plane or others)
  // 
};
