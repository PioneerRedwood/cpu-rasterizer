/**
 * SDLProgram
 * 
 */

#pragma once

#include <SDL.h>
#include "SDLRenderer.hpp"

struct SDLProgram {
    ~SDLProgram() {
        quit = true;

        delete renderer;

        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    int initialize(int width, int height) {
        if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
            return -1;
        }

        window = SDL_CreateWindow("cpu-rasterizer", SDL_WINDOWPOS_CENTERED, 
            SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
        if(window == nullptr) {
            return -1;
        }

        screenWidth = width;
        screenHeight = height;

        renderer = new SDLRenderer(window, width, height);

        return 0;
    }

    void render(float delta) {
        renderer->render(delta);
    }

    void updateTime() {
        lastTime = currentTime;
        currentTime = SDL_GetPerformanceCounter();
        delta = (double) ((currentTime - lastTime) * 1000 /
                          (double) SDL_GetPerformanceFrequency());
    }

    void handleKeyInput(SDL_Event event) {
        // Propaganda the key event invoked
    }

    void handlePollEvent() {
        SDL_Event event;
        while(SDL_PollEvent(&event) != 0) {
            switch (event.type) {
                case SDL_QUIT: {
                    quit = true;
                    break;
                }
                case SDL_KEYDOWN: {
                    handleKeyInput(event);
                    break;
                }
                default: {
                    break;
                }
            }
        }
    }

    int run() {
        while(not quit) {
            updateTime();
            handlePollEvent();
            render(delta);

            SDL_Delay(1);
        }

        return 0;
    }

private:
    SDL_Window* window { nullptr };
    bool quit { false };
    
    uint64_t currentTime { 0 };
    uint64_t lastTime { 0 };
    double delta { 0.0f };

    int screenWidth { 0 };
    int screenHeight { 0 };
    
    SDLRenderer* renderer { nullptr };
};
