#pragma once

#include <SDL.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "Log.hpp"
#include "Color.hpp"
#include "TGA.hpp"

class TextureLoader
{
public:
  explicit TextureLoader(const char *suffix)
  {
    m_ResourceDirectorySuffix = std::string(suffix);
  }

  ~TextureLoader()
  {
    for (auto *tga : m_LoadedTga)
    {
      if (tga)
      {
        delete tga;
        tga = nullptr;
      }
    }
  }

  static bool FileExist(const std::filesystem::path &filepath)
  {
    std::error_code ec;
    return std::filesystem::exists(filepath, ec) &&
           std::filesystem::is_regular_file(filepath, ec);
  }

  uint32_t GetSupportedTextureFormat(SDL_Renderer *renderer,
                                   uint32_t fallbackFormat)
  {
    // fallbackFormat "SDL_PIXELFORMAT_RGBA32"
    if (m_SupportedTextureFormat != -1)
    {
      return m_SupportedTextureFormat;
    }

    if (renderer == nullptr)
    {
      return fallbackFormat;
    }

    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(renderer, &info) != 0 || info.num_texture_formats == 0)
    {
      return fallbackFormat;
    }

    for (uint32_t i = 0; i < info.num_texture_formats; ++i)
    {
      const uint32_t format = info.texture_formats[i];
      if (!SDL_ISPIXELFORMAT_FOURCC(format) && SDL_BYTESPERPIXEL(format) == 4)
      {
        return format;
      }
    }

    m_SupportedTextureFormat = info.texture_formats[0];

    return m_SupportedTextureFormat;
  }

  TGA *LoadTGATextureWithName(SDL_Renderer *renderer, const char *name,
                              uint32_t textureFormat = SDL_PIXELFORMAT_RGBA32)
  {
    if (name == nullptr || name[0] == '\0')
    {
      return nullptr;
    }
    const uint32_t supportedTextureFormat =
        GetSupportedTextureFormat(renderer, textureFormat);

    namespace fs = std::filesystem;

    std::vector<fs::path> searchRoots;
    searchRoots.emplace_back(fs::current_path());

    if (char *basePath = SDL_GetBasePath(); basePath != nullptr)
    {
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

    for (const fs::path &root : searchRoots)
    {
      for (const fs::path &dir : candidates)
      {
        if (dir.empty())
        {
          continue;
        }

        const fs::path filename = root / dir / name;
        attempted.push_back(filename.lexically_normal().string());
        if (!FileExist(filename))
        {
          continue;
        }

        TGA *tga = new TGA();
        const std::string texturePath = filename.string();
        if (!tga->ReadFromFile(texturePath.c_str(), supportedTextureFormat))
        {
          delete tga;
          continue;
        }

        if (!tga->CreateTexture(renderer, supportedTextureFormat))
        {
          LogF("[DEBUG] Create texture has failed ");
        }
        m_LoadedTga.push_back(tga);
        return tga;
      }
    }

    LogF("Failed to load texture file %s", name);
    for (const std::string &path : attempted)
    {
      LogF("Tried: %s", path.c_str());
    }
    return nullptr;
  }

  // The simplified bunny mesh object is from
  // https://graphics.stanford.edu/~mdfisher/Data/Meshes/bunny.obj
  SimpleMesh *LoadSimpleMeshFromObj(const char *filepath)
  {
    if (filepath == nullptr)
    {
      return nullptr;
    }

    LogF("[DEBUG] Attempting to load mesh from %s", filepath);

    FILE *fp = fopen(filepath, "r");
    if (fp == nullptr)
    {
      LogF("[DEBUG] Failed to open %s", filepath);
      return nullptr;
    }

    uint32_t vertexCount = 0;
    uint32_t faceCount = 0;

    SimpleMesh *mesh = new SimpleMesh();
    mesh->tga = nullptr;

    char line[256];
    if (fgets(line, sizeof(line), fp) != nullptr)
    {
      // This bunny file starts with: "<vertexCount> <faceCount>".
      if (sscanf(line, "%u %u", &vertexCount, &faceCount) == 2)
      {
        mesh->verts.reserve(vertexCount);
        mesh->indices.reserve(faceCount * 3);
      }
      else
      {
        // Standard OBJ files may not have this header line.
        rewind(fp);
      }
    }

    while (fgets(line, sizeof(line), fp))
    {
      if (line[0] == 'v' && std::isspace(static_cast<unsigned char>(line[1])))
      {
        Vector3 p{};
        if (sscanf(line, "v %f %f %f", &p.x, &p.y, &p.z) == 3)
        {
          mesh->verts.push_back(p);
        }
      }
      else if (line[0] == 'f' &&
               std::isspace(static_cast<unsigned char>(line[1])))
      {
        uint32_t i0 = 0;
        uint32_t i1 = 0;
        uint32_t i2 = 0;
        if (sscanf(line, "f %u %u %u", &i0, &i1, &i2) == 3 &&
            i0 > 0 && i1 > 0 && i2 > 0)
        {
          // OBJ indices are 1-based.
          mesh->indices.push_back(i0 - 1);
          mesh->indices.push_back(i1 - 1);
          mesh->indices.push_back(i2 - 1);
        }
      }
    }

    fclose(fp);

    if (vertexCount != 0 && mesh->verts.size() != vertexCount)
    {
      LogF("[DEBUG] Vertex count mismatch. header=%u parsed=%zu", vertexCount,
           mesh->verts.size());
    }
    if (faceCount != 0 && (mesh->indices.size() / 3) != faceCount)
    {
      LogF("[DEBUG] Face count mismatch. header=%u parsed=%zu", faceCount,
           mesh->indices.size() / 3);
    }

    return mesh;
  }

private:
  std::string m_ResourceDirectorySuffix;
  std::vector<TGA *> m_LoadedTga;
  uint32_t m_SupportedTextureFormat;
};
