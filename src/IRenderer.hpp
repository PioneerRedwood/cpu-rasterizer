#pragma once

#include <SDL.h>

class IRenderer {
public:
  IRenderer(SDL_Window* window, int width, int height) {}
  ~IRenderer() {};

  virtual void Render(double delta) = 0;
  virtual void HandleKeyInput(const SDL_Event& event) = 0;
};
