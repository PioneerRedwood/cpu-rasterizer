#include "GLRenderer.hpp"
#include "Log.hpp"

namespace {
constexpr const char* s_vsh = R"(#version 150

in vec3 position;

void main()
{
  gl_Position = vec4(position, 1.0);
}
)";

constexpr const char* s_fsh = R"(#version 150

out vec4 fragColor;

void main()
{
  fragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
)";

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

}

GLRenderer::GLRenderer(SDL_Window* window, int width, int height) 
: IRenderer(window, width, height),
  m_Window(window),
  m_Width(width),
  m_Height(height)
{
  LoadResources();
}

GLRenderer::~GLRenderer()
{
  if (m_RenderCubeProgram != 0)
  {
    glDeleteProgram(m_RenderCubeProgram);
  }
  if (m_VertexShader != 0)
  {
    glDeleteShader(m_VertexShader);
  }
  if (m_FragmentShader != 0)
  {
    glDeleteShader(m_FragmentShader);
  }
  if (m_Ebo != 0)
  {
    glDeleteBuffers(1, &m_Ebo);
  }
  if (m_Vbo != 0)
  {
    glDeleteBuffers(1, &m_Vbo);
  }
  if (m_Vao != 0)
  {
    glDeleteVertexArrays(1, &m_Vao);
  }
}

void GLRenderer::HandleKeyInput(const SDL_Event& event) 
{

}

void GLRenderer::Render(double delta)
{
  (void)delta;
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
  if (m_RenderCubeProgram == 0 || m_Vao == 0 || m_CubeIndexCount == 0)
  {
    return;
  }

  glUseProgram(m_RenderCubeProgram);
  glBindVertexArray(m_Vao);
  glDrawElements(GL_TRIANGLES, m_CubeIndexCount, GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(0);
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

  if (!CreateGPUBuffers())
  {
    return;
  }

  BuildCube();
  glEnable(GL_DEPTH_TEST);
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

bool GLRenderer::CreateGPUBuffers()
{
  glGenVertexArrays(1, &m_Vao);
  glGenBuffers(1, &m_Vbo);
  glGenBuffers(1, &m_Ebo);

  GLenum err = glGetError();
  if(err != GL_NO_ERROR) {
    LogF("[GL] GLRenderer::CreateGPUBuffers() failed in creating buffers! %u", err);
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
  glLinkProgram(m_RenderCubeProgram);

  return CheckProgramLink(m_RenderCubeProgram, "RenderCubeProgram");
}

void GLRenderer::BuildCube()
{
  constexpr GLfloat vertices[] = {
      -0.5f, -0.5f, -0.5f,
       0.5f, -0.5f, -0.5f,
       0.5f,  0.5f, -0.5f,
      -0.5f,  0.5f, -0.5f,
      -0.5f, -0.5f,  0.5f,
       0.5f, -0.5f,  0.5f,
       0.5f,  0.5f,  0.5f,
      -0.5f,  0.5f,  0.5f,
  };

  constexpr GLuint indices[] = {
      0, 1, 2, 2, 3, 0,
      4, 5, 6, 6, 7, 4,
      0, 4, 7, 7, 3, 0,
      1, 5, 6, 6, 2, 1,
      3, 2, 6, 6, 7, 3,
      0, 1, 5, 5, 4, 0,
  };

  m_CubeIndexCount = static_cast<GLsizei>(sizeof(indices) / sizeof(indices[0]));

  glBindVertexArray(m_Vao);

  glBindBuffer(GL_ARRAY_BUFFER, m_Vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);

  glBindVertexArray(0);
}
