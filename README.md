# cpu-rasterizer

CPU Rasterizer is a simple software rendering demo using SDL2.

## How to build on Windows

Prerequisites:
- CMake 3.19 or newer
- Visual Studio 2022 with C++ build tools

From the repository root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

Run:

```powershell
.\build\Debug\cpu-rasterizer.exe
```

## How to build on macOS

Prerequisites:
- CMake 3.19 or newer
- Xcode command line tools (`xcode-select --install`)

From the repository root:

```bash
cmake -S . -B build -G Xcode
cmake --build build --config Debug
```

Note:
- The `resources` directory is copied next to the built executable, so texture files are found even when Xcode uses a DerivedData output path.

Run:

```bash
./build/Debug/cpu-rasterizer
```
