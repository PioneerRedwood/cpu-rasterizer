/**
 * SDLProgram
 * 
 */

#pragma once

struct WorldCamera;
struct SDLRenderer;

struct SDLProgram {
    int screenWidth { 0 };
    int screenHeight { 0 };
    
    unsigned int* framebuffer { nullptr };
    WorldCamera* camera { nullptr };
    SDLRenderer* renderer { nullptr };

    void initialize(int width, int height);

    void deinitialize();

    void render(float delta);
};
