#pragma once

#include "Color.hpp"
#include "Log.hpp"
#include "TGA.hpp"
#include "FileSystem.hpp"

#include <SDL.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

class TextureLoader {
 public:
  explicit TextureLoader(SDL_Renderer& renderer)
  : m_Renderer(renderer) {
    m_ResourceDirectoryPrefix = std::string(CPURASTERIZER_RESOURCE_DIR);
  }

  ~TextureLoader() {
    for (auto* tga : m_LoadedTga) {
      if (tga) {
        delete tga;
        tga = nullptr;
      }
    }
  }

  uint32_t GetSupportedTextureFormat(uint32_t fallbackFormat) {
    // fallbackFormat "SDL_PIXELFORMAT_RGBA32"
    if (m_SupportedTextureFormat != -1) {
      return m_SupportedTextureFormat;
    }

    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(&m_Renderer, &info) != 0 ||
        info.num_texture_formats == 0) {
      return fallbackFormat;
    }

    for (uint32_t i = 0; i < info.num_texture_formats; ++i) {
      const uint32_t format = info.texture_formats[i];
      if (!SDL_ISPIXELFORMAT_FOURCC(format) && SDL_BYTESPERPIXEL(format) == 4) {
        return format;
      }
    }

    m_SupportedTextureFormat = info.texture_formats[0];

    return m_SupportedTextureFormat;
  }

  TGA* LoadTGATextureWithName(const char* name,
                              uint32_t textureFormat = SDL_PIXELFORMAT_RGBA32) {
    if (name == nullptr || name[0] == '\0') {
      return nullptr;
    }
    const uint32_t supportedTextureFormat = GetSupportedTextureFormat(textureFormat);

    namespace fs = std::filesystem;
    const fs::path filename = m_ResourceDirectoryPrefix + "/" + name;

    if (!FileExist(filename)) {
      LogF("[DEBUG] Create texture has failed : file does not exist!");
      return nullptr;
    }

    TGA* tga = new TGA();
    const std::string texturePath = filename.string();
    if (!tga->ReadFromFile(texturePath.c_str(), supportedTextureFormat)) {
      LogF("[DEBUG] Create texture has failed : reading the file fail!");
      delete tga;
      return nullptr;
    }

    //        if (!tga->CreateTexture(renderer, supportedTextureFormat))
    //        {
    //          LogF("[DEBUG] Create texture has failed ");
    //        }
    m_LoadedTga.push_back(tga);
    return tga;
  }

  SDL_Renderer& m_Renderer;
  std::string m_ResourceDirectoryPrefix;
  std::vector<TGA*> m_LoadedTga;
  uint32_t m_SupportedTextureFormat;
};
