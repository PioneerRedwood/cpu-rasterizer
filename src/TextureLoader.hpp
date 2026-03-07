#pragma once

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "Log.hpp"
#include "Color.hpp"
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

  static Uint32 GetSupportedTextureFormat(SDL_Renderer* renderer,
                                          Uint32 fallbackFormat) {
    if (renderer == nullptr) {
      return fallbackFormat;
    }

    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(renderer, &info) != 0 || info.num_texture_formats == 0) {
      return fallbackFormat;
    }

    for (Uint32 i = 0; i < info.num_texture_formats; ++i) {
      const Uint32 format = info.texture_formats[i];
      if (!SDL_ISPIXELFORMAT_FOURCC(format) && SDL_BYTESPERPIXEL(format) == 4) {
        return format;
      }
    }

    return info.texture_formats[0];
  }

  TGA* LoadTGATextureWithName(SDL_Renderer* renderer, const char* name,
                              Uint32 textureFormat = SDL_PIXELFORMAT_RGBA32) {
    if (name == nullptr || name[0] == '\0') {
      return nullptr;
    }
    const Uint32 supportedTextureFormat =
        GetSupportedTextureFormat(renderer, textureFormat);

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
        if (!tga->ReadFromFile(texturePath.c_str(), supportedTextureFormat)) {
          delete tga;
          continue;
        }

        if (!tga->CreateTexture(renderer, supportedTextureFormat)) {
          LogF("[DEBUG] Create texture has failed ");
        }
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
