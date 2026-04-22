/**
 * Program
 */

#pragma once

#include <SDL.h>

#include "Log.hpp"

#define SOFTWARE_RENDERING_ENABLED 0

#if SOFTWARE_RENDERING_ENABLED
#include "Renderer.hpp"
#else
#include "GLRenderer.hpp"
#endif

#if __has_include("DebugDump.h")
#include "DebugDump.h"
#else
namespace debugdump {
inline Uint32 WindowFlags(Uint32 normalFlags) { return normalFlags; }
inline bool ShouldQuitAfterFrame() { return false; }
}  // namespace debugdump
#endif

class Program {
 public:
  ~Program() {
    m_Quit = true;

    delete m_Renderer;
#if not SOFTWARE_RENDERING_ENABLED
    SDL_GL_DeleteContext(m_GLContext);
#endif

    SDL_DestroyWindow(m_Window);
    SDL_Quit();
  }

  int Initialize(int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
      LogF("SDL could not initialize! SDL_Error: %s", SDL_GetError());
      return -1;
    }
    
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

#if defined(__APPLE__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

    m_Window = SDL_CreateWindow("cpu-rasterizer", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, width, height,
                              debugdump::WindowFlags(SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL));
    if (m_Window == nullptr) {
      LogF("Window could not be created! SDL_Error: %s", SDL_GetError());
      return -1;
    }

#if not SOFTWARE_RENDERING_ENABLED
    m_GLContext = SDL_GL_CreateContext(m_Window);
    if(m_GLContext == nullptr) {
      LogF("Creating GLContext has failed! SDL_Error: %s", SDL_GetError());
      return -1;
    }

    int result = SDL_GL_MakeCurrent(m_Window, m_GLContext);
    if(result != 0) {
      LogF("Set current GLContext has failed SDL_Error: %s", SDL_GetError());
      return -1;
    }
#endif

    m_ScreenWidth = width;
    m_ScreenHeight = height;

#if SOFTWARE_RENDERING_ENABLED
    m_Renderer = new Renderer(m_Window, width, height);
#else
    m_Renderer = new GLRenderer(m_Window, width, height);
#endif
    return 0;
  }

  int Run() {
    LogF("Start main loop");
    while (not m_Quit) {
      UpdateTime();
      HandlePollEvent();
      Render(m_Delta);

      if (debugdump::ShouldQuitAfterFrame()) {
        m_Quit = true;
      }

      SDL_Delay(1);
    }

    return 0;
  }

 private:
  void Render(float delta) { m_Renderer->Render(delta); }

  void UpdateTime() {
    m_LastTime = m_CurrentTime;
    m_CurrentTime = SDL_GetPerformanceCounter();
    m_Delta = (double)((m_CurrentTime - m_LastTime) * 1000 /
                     (double)SDL_GetPerformanceFrequency());
  }

  void HandleKeyInput(const SDL_Event& event) {
    m_Renderer->HandleKeyInput(event);
  }

  void HandlePollEvent() {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      switch (event.type) {
        case SDL_QUIT: {
          m_Quit = true;
          break;
        }
        case SDL_KEYDOWN: {
          HandleKeyInput(event);
          break;
        }
        default: {
          break;
        }
      }
    }
  }

 private:
  SDL_Window* m_Window{nullptr};
  bool m_Quit{false};

  uint64_t m_CurrentTime{0};
  uint64_t m_LastTime{0};
  double m_Delta{0.0f};

  int m_ScreenWidth{0};
  int m_ScreenHeight{0};
#if SOFTWARE_RENDERING_ENABLED
  IRenderer* m_Renderer {nullptr};
#else
  GLRenderer* m_Renderer {nullptr};
  SDL_GLContext m_GLContext;
#endif
};
