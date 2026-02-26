#pragma once

#include <SDL.h>
#include "WorldCamera.hpp"

struct SDLRenderer {
    SDLRenderer(SDL_Window* window, int width, int height)
    : width(width), height(height) {
        framebuffer = new unsigned int[width * height];
        camera = new WorldCamera();

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }

    ~SDLRenderer() {
        delete[] framebuffer;
        SDL_DestroyRenderer(renderer);
    }

    void render(double delta) {
        // TODO: 
    }

    int width;
    int height;
    unsigned int* framebuffer;
    WorldCamera* camera;

    SDL_Renderer* renderer;
};
