#include "SDLProgram.hpp"
#include "WorldCamera.hpp"
#include "SDLRenderer.hpp"

void SDLProgram::initialize(int width, int height) {
    screenWidth = width;
    screenHeight = height;

    camera = new WorldCamera();
    framebuffer = new unsigned int[width * height];
    renderer = new SDLRenderer(width, height, framebuffer);
}