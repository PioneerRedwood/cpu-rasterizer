#pragma once

#include <SDL.h>
#include "WorldCamera.hpp"

struct SDLRenderer {
    SDLRenderer(SDL_Window* window, int width, int height)
    : width(width), height(height) {
        framebuffer = new unsigned int[width * height];
        camera = new WorldCamera();

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        mainTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, 
            SDL_TEXTUREACCESS_STREAMING, width, height);
        
        camera->aspect = (float) width / height;
        camera->fov = 45.0f;

        setupMatrices();

        buildTriangle();
    }

    ~SDLRenderer() {
        delete[] framebuffer;
        SDL_DestroyRenderer(renderer);
    }

    void buildTriangle() {
        // Build some vertices for drawing triangle
        tri[0] = { -1.0f, -1.0f, +0.0f };
        tri[1] = { +1.0f, -1.0f, +0.0f };
        tri[2] = { +0.0f, +1.0f, +0.0f };
    }

    void renderTriangleLines() {
        // TODO: Set transform of each verties of triangle
        //  draw lines using edge function
    }

    void transformToScreen(Vector4& point) {
        point = projectionMatrix * (cameraMatrix * point);

        point.perspectiveDivide();

        point = viewportMatrix * point;
    }

    void setupMatrices() {
        math::setupCameraMatrix(cameraMatrix, camera->eye, camera->at, camera->up);
        math::setupPerspectiveProjectionMatrix(projectionMatrix, camera->fov, 
            camera->aspect, kZNear, kZFar);
        math::setupViewportMatrix(viewportMatrix, 0, 0, width, height, kZNear, kZFar);
    }

    void render(double delta) {
        // TODO: 

        memset((char*)framebuffer, 0, sizeof(int) * width * height);

        renderTriangleLines();

        SDL_UpdateTexture(mainTexture, nullptr, framebuffer, width * 4);
        SDL_RenderCopy(renderer, mainTexture, nullptr, nullptr);
        SDL_RenderFlush(renderer);
        SDL_Delay(1);
    }

    int width;
    int height;
    unsigned int* framebuffer;
    WorldCamera* camera;

    Matrix4x4 viewportMatrix, projectionMatrix, cameraMatrix;

    SDL_Renderer* renderer;
    SDL_Texture* mainTexture;

    const float kZNear = 0.1f, kZFar = 10.0f;

    // Triangles
    Vector3 tri[3];
};
