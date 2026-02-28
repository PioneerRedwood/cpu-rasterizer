#pragma once

#include <SDL.h>
#include "WorldCamera.hpp"

struct SDLRenderer
{
    SDLRenderer(SDL_Window *window, int width, int height)
        : width(width), height(height)
    {
        framebuffer = new unsigned int[width * height];
        camera = new WorldCamera();

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        mainTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_STREAMING, width, height);

        camera->aspect = (float)width / height;
        camera->fov = 45.0f;

        setupMatrices();

        buildTriangle();
    }

    ~SDLRenderer()
    {
        delete[] framebuffer;
        SDL_DestroyTexture(mainTexture);
        SDL_DestroyRenderer(renderer);
    }

    void buildTriangle()
    {
        // Build some vertices for drawing triangle
        triVerts[0] = { -1.0f, +1.0f, +0.0f };
        triVerts[1] = { +1.0f, +1.0f, +0.0f };
        triVerts[2] = { +0.0f, -1.0f, +0.0f };
    }

    void drawPoint(int x, int y, int color)
    {
        if (x >= width || x < 0)
            return;
        if (y >= height || y < 0)
            return;

        framebuffer[x + y * width] = color;
    }

    /**
     * Draw line with Bresenham algorithm
     */
    void drawLine(const Vector2 &startPos, const Vector2 &endPos, int color)
    {
        auto drawLow = [this](int x0, int y0, int x1, int y1, int color)
        {
            int dx = x1 - x0, dy = y1 - y0;
            int yi = 1;
            if (dy < 0)
            {
                yi = -1;
                dy = -dy;
            }
            int d = (2 * dy) - dx;
            int y = y0;

            for (int x = x0; x < x1; ++x)
            {
                drawPoint(x, y, color);
                if (d > 0)
                {
                    y = y + yi;
                    d = d + (2 * (dy - dx));
                }
                else
                {
                    d = d + 2 * dy;
                }
            }
        };

        auto drawHigh = [this](int x0, int y0, int x1, int y1, int color)
        {
            int dx = x1 - x0, dy = y1 - y0;
            int xi = 1;
            if (dx < 0)
            {
                xi = -1;
                dx = -dx;
            }
            int d = (2 * dx) - dy;
            int x = x0;

            for (int y = y0; y < y1; ++y)
            {
                drawPoint(x, y, color);
                if (d > 0)
                {
                    x = x + xi;
                    d = d + (2 * (dx - dy));
                }
                else
                {
                    d = d + 2 * dx;
                }
            }
        };

        if (abs(endPos.y - startPos.y) < abs(endPos.x - startPos.x))
        {
            if (startPos.x > endPos.x)
            {
                drawLow(endPos.x, endPos.y, startPos.x, startPos.y, color);
            }
            else
            {
                drawLow(startPos.x, startPos.y, endPos.x, endPos.y, color);
            }
        }
        else
        {
            if (startPos.y > endPos.y)
            {
                drawHigh(endPos.x, endPos.y, startPos.x, startPos.y, color);
            }
            else
            {
                drawHigh(startPos.x, startPos.y, endPos.x, endPos.y, color);
            }
        }
    }

    void renderTriangleLines()
    {
        // Matrix4x4 rotateMat = Matrix4x4::identity;
        // triRotateRadian += 0.6f;
        // rotateMat.rotateY(triRotateRadian);

        Vector3 tri[3];
        for(int i = 0; i < 3; ++i) {
            Vector4 v = { triVerts[i].x, triVerts[i].y, triVerts[i].z, 1.0f };
            transformToScreen(v);
            tri[i].x = v.x, tri[i].y = v.y, tri[i].z = v.z;
        }

        const int whiteColor = 0xFFFFFFFF;
        drawLine( { tri[0].x, tri[0].y }, { tri[1].x, tri[1].y }, whiteColor );
        drawLine( { tri[1].x, tri[1].y }, { tri[2].x, tri[2].y }, whiteColor );
        drawLine( { tri[0].x, tri[0].y }, { tri[2].x, tri[2].y }, whiteColor );
    }

    void transformToScreen(Vector4 &point)
    {
        point = projectionMatrix * (cameraMatrix * point);
        point.perspectiveDivide();
        point = viewportMatrix * point;
    }

    void setupMatrices()
    {
        math::setupCameraMatrix(cameraMatrix, camera->eye, camera->at, camera->up);
        math::setupPerspectiveProjectionMatrix(projectionMatrix, camera->fov,
                                               camera->aspect, kZNear, kZFar);
        math::setupViewportMatrix(viewportMatrix, 0, 0, width, height, kZNear, kZFar);
    }

    void render(double delta)
    {
        memset((char *)framebuffer, 0, sizeof(int) * width * height);

        renderTriangleLines();

        SDL_UpdateTexture(mainTexture, nullptr, framebuffer, width * 4);
        SDL_RenderCopy(renderer, mainTexture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        SDL_Delay(1);
    }

    int width;
    int height;
    unsigned int *framebuffer;
    WorldCamera *camera;

    Matrix4x4 viewportMatrix, projectionMatrix, cameraMatrix;

    SDL_Renderer *renderer;
    SDL_Texture *mainTexture;

    const float kZNear = 0.1f, kZFar = 10.0f;

    // Start Draw Tri
    Vector3 triVerts[3];
    float triRotateRadian { 0.0f };
    // End Draw Tri
};
