#include "SDLProgram.hpp"

int main(int argc, char** argv) {
    SDLProgram program;
    constexpr int width = 800, height = 600;
    if(program.initialize(width, height) != 0) {
        return -1;
    }
    return program.run();
}