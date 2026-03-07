#pragma once

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "Log.hpp"
#include "RGBA.hpp"
#include "TGA.hpp"

class TextureLoader {
 public:
  explicit TextureLoader(const char* suffix) {
    m_ResourceDirectorySuffix = std::string(suffix);
  }

  ~TextureLoader() {
    for (auto* tga : m_LoadedTga) {
      if (tga) {
        delete tga;
        tga = nullptr;
      }
    }
  }

  static bool FileExist(const std::filesystem::path& filepath) {
    std::error_code ec;
    return std::filesystem::exists(filepath, ec) &&
           std::filesystem::is_regular_file(filepath, ec);
  }

  TGA* LoadTGATextureWithName(SDL_Renderer* renderer, const char* name) {
    if (name == nullptr || name[0] == '\0') {
      return nullptr;
    }

    namespace fs = std::filesystem;

    std::vector<fs::path> searchRoots;
    searchRoots.emplace_back(fs::current_path());

    if (char* basePath = SDL_GetBasePath(); basePath != nullptr) {
      const fs::path executableBase(basePath);
      SDL_free(basePath);
      searchRoots.emplace_back(executableBase);
      searchRoots.emplace_back(executableBase / ".." / "Resources");
    }

#if defined(CPURASTERIZER_SOURCE_DIR)
    searchRoots.emplace_back(fs::path(CPURASTERIZER_SOURCE_DIR));
#endif

    const std::vector<fs::path> candidates = {
        fs::path(m_ResourceDirectorySuffix), fs::path("resources"),
        fs::path("../resources"), fs::path("../../resources")};

    std::vector<std::string> attempted;

    for (const fs::path& root : searchRoots) {
      for (const fs::path& dir : candidates) {
        if (dir.empty()) {
          continue;
        }

        const fs::path filename = root / dir / name;
        attempted.push_back(filename.lexically_normal().string());
        if (!FileExist(filename)) {
          continue;
        }

        TGA* tga = new TGA();
        const std::string texturePath = filename.string();
        if (!tga->ReadFromFile(texturePath.c_str())) {
          delete tga;
          continue;
        }

        tga->CreateTexture(renderer);
        m_LoadedTga.push_back(tga);
        return tga;
      }
    }

    LogF("Failed to load texture file %s", name);
    for (const std::string& path : attempted) {
      LogF("Tried: %s", path.c_str());
    }
    return nullptr;
  }

 private:
  std::string m_ResourceDirectorySuffix;
  std::vector<TGA*> m_LoadedTga;
};
