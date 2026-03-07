/**
 * Program
 *
 */

#pragma once

#include <SDL.h>

#include "Log.hpp"
#include "Renderer.hpp"

class Program {
 public:
  ~Program() {
    quit = true;

    delete renderer;

    SDL_DestroyWindow(window);
    SDL_Quit();
  }

  int Initialize(int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
      LogF("SDL could not initialize! SDL_Error: %s", SDL_GetError());
      return -1;
    }

    window = SDL_CreateWindow("cpu-rasterizer", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, width, height,
                              SDL_WINDOW_SHOWN);
    if (window == nullptr) {
      LogF("Window could not be created! SDL_Error: %s", SDL_GetError());
      return -1;
    }

    screenWidth = width;
    screenHeight = height;

    renderer = new Renderer(window, width, height);

    return 0;
  }

  int Run() {
    LogF("Start main loop");
    while (not quit) {
      UpdateTime();
      HandlePollEvent();
      Render(delta);

      SDL_Delay(1);
    }

    return 0;
  }

 private:
  void Render(float delta) { renderer->Render(delta); }

  void UpdateTime() {
    lastTime = currentTime;
    currentTime = SDL_GetPerformanceCounter();
    delta = (double)((currentTime - lastTime) * 1000 /
                     (double)SDL_GetPerformanceFrequency());
  }

  void HandleKeyInput(SDL_Event event) {
    // Propaganda the key event invoked
  }

  void HandlePollEvent() {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      switch (event.type) {
        case SDL_QUIT: {
          quit = true;
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
  SDL_Window* window{nullptr};
  bool quit{false};

  uint64_t currentTime{0};
  uint64_t lastTime{0};
  double delta{0.0f};

  int screenWidth{0};
  int screenHeight{0};

  Renderer* renderer{nullptr};
};
