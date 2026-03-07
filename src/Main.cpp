#include "Program.hpp"

int main(int argc, char** argv) {
  Program program;
  constexpr int width = 800, height = 600;
  if (program.Initialize(width, height) != 0) {
    return -1;
  }
  return program.Run();
}