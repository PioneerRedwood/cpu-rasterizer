#pragma once

#include <SDL.h>

#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Log.hpp"
#include "Color.hpp"
#include "Mesh.hpp"
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
  Mesh *LoadSimpleMeshFromObj(const char *filepath)
  {
    if (filepath == nullptr)
    {
      return nullptr;
    }

    LogF("[DEBUG] Attempting to load mesh from %s", filepath);

    namespace fs = std::filesystem;
    const fs::path requestedPath(filepath);

    std::vector<fs::path> searchRoots;
    searchRoots.emplace_back(fs::current_path());

    if (char *basePath = SDL_GetBasePath(); basePath != nullptr)
    {
      const fs::path executableBase(basePath);
      SDL_free(basePath);
      searchRoots.emplace_back(executableBase);
      searchRoots.emplace_back(executableBase / "..");
      searchRoots.emplace_back(executableBase / ".." / "..");
    }

#if defined(CPURASTERIZER_SOURCE_DIR)
    searchRoots.emplace_back(fs::path(CPURASTERIZER_SOURCE_DIR));
#endif

    std::vector<fs::path> candidates;
    if (requestedPath.is_absolute())
    {
      candidates.push_back(requestedPath);
    }
    else
    {
      candidates.push_back(requestedPath);
      candidates.push_back(fs::path("resources") / requestedPath.filename());
      candidates.push_back(fs::path("../resources") / requestedPath.filename());
      candidates.push_back(fs::path("../../resources") / requestedPath.filename());
    }

    fs::path resolvedPath;
    for (const fs::path &root : searchRoots)
    {
      for (const fs::path &candidate : candidates)
      {
        const fs::path fullPath =
            candidate.is_absolute() ? candidate : (root / candidate);
        if (FileExist(fullPath))
        {
          resolvedPath = fullPath.lexically_normal();
          break;
        }
      }
      if (!resolvedPath.empty())
      {
        break;
      }
    }

    if (resolvedPath.empty())
    {
      LogF("[DEBUG] Failed to resolve mesh path for %s", filepath);
      return nullptr;
    }

    FILE *fp = fopen(resolvedPath.string().c_str(), "r");
    if (fp == nullptr)
    {
      LogF("[DEBUG] Failed to open %s", resolvedPath.string().c_str());
      return nullptr;
    }

    uint32_t vertexCount = 0;
    uint32_t faceCount = 0;

    Mesh *mesh = new Mesh();
    std::vector<Vector3> rawPositions;
    std::vector<Vector2> rawUVs;
    std::vector<Vector3> rawNormals;
    std::unordered_map<ObjVertexKey, uint32_t, ObjVertexKeyHasher> vertexMap;
    bool allFacesHaveNormals = true;

    char line[256];
    if (fgets(line, sizeof(line), fp) != nullptr)
    {
      // This bunny file starts with: "<vertexCount> <faceCount>".
      if (sscanf(line, "%u %u", &vertexCount, &faceCount) == 2)
      {
        rawPositions.reserve(vertexCount);
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
          rawPositions.push_back(p);
        }
      }
      else if (line[0] == 'v' &&
               std::isspace(static_cast<unsigned char>(line[1])) == 0 &&
               line[1] == 't' &&
               std::isspace(static_cast<unsigned char>(line[2])))
      {
        Vector2 uv{};
        if (sscanf(line, "vt %f %f", &uv.x, &uv.y) >= 2)
        {
          rawUVs.push_back(uv);
        }
      }
      else if (line[0] == 'v' &&
               std::isspace(static_cast<unsigned char>(line[1])) == 0 &&
               line[1] == 'n' &&
               std::isspace(static_cast<unsigned char>(line[2])))
      {
        Vector3 n{};
        if (sscanf(line, "vn %f %f %f", &n.x, &n.y, &n.z) == 3)
        {
          rawNormals.push_back(n);
        }
      }
      else if (line[0] == 'f' &&
               std::isspace(static_cast<unsigned char>(line[1])))
      {
        std::istringstream iss(line + 1);
        std::string token;
        std::vector<ObjVertexKey> face;
        while (iss >> token)
        {
          ObjVertexKey key{};
          if (!ParseObjFaceToken(token, key))
          {
            face.clear();
            break;
          }
          face.push_back(key);
        }

        if (face.size() < 3)
        {
          continue;
        }

        for (size_t i = 1; i + 1 < face.size(); ++i)
        {
          if (!AppendObjVertex(face[0], rawPositions, rawUVs, rawNormals,
                               vertexMap, *mesh) ||
              !AppendObjVertex(face[i], rawPositions, rawUVs, rawNormals,
                               vertexMap, *mesh) ||
              !AppendObjVertex(face[i + 1], rawPositions, rawUVs, rawNormals,
                               vertexMap, *mesh))
          {
            continue;
          }

          if (face[0].vn == 0 || face[i].vn == 0 || face[i + 1].vn == 0)
          {
            allFacesHaveNormals = false;
          }
        }
      }
    }

    fclose(fp);

    if (vertexCount != 0 && rawPositions.size() != vertexCount)
    {
      LogF("[DEBUG] Vertex count mismatch. header=%u parsed=%zu", vertexCount,
           rawPositions.size());
    }
    if (faceCount != 0 && (mesh->indices.size() / 3) != faceCount)
    {
      LogF("[DEBUG] Face count mismatch. header=%u parsed=%zu", faceCount,
           mesh->indices.size() / 3);
    }

    if (!mesh->verts.empty() && (!allFacesHaveNormals || rawNormals.empty()))
    {
      GenerateNormals(*mesh);
    }
    else if (!mesh->verts.empty())
    {
      mesh->hasNormals = true;
    }

    return mesh;
  }

private:
  struct ObjVertexKey
  {
    int v{0};
    int vt{0};
    int vn{0};

    bool operator==(const ObjVertexKey &other) const
    {
      return v == other.v && vt == other.vt && vn == other.vn;
    }
  };

  struct ObjVertexKeyHasher
  {
    size_t operator()(const ObjVertexKey &key) const
    {
      size_t h = static_cast<size_t>(key.v);
      h = (h * 1315423911u) ^ static_cast<size_t>(key.vt + 0x9e3779b9);
      h = (h * 1315423911u) ^ static_cast<size_t>(key.vn + 0x85ebca6b);
      return h;
    }
  };

  static bool ParseObjInt(const std::string &text, int &value)
  {
    if (text.empty())
    {
      return false;
    }

    char *end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0')
    {
      return false;
    }

    value = static_cast<int>(parsed);
    return true;
  }

  static bool ParseObjFaceToken(const std::string &token, ObjVertexKey &out)
  {
    out = {};

    const size_t firstSlash = token.find('/');
    if (firstSlash == std::string::npos)
    {
      return ParseObjInt(token, out.v);
    }

    const std::string vText = token.substr(0, firstSlash);
    if (!ParseObjInt(vText, out.v))
    {
      return false;
    }

    const size_t secondSlash = token.find('/', firstSlash + 1);
    if (secondSlash == std::string::npos)
    {
      const std::string vtText = token.substr(firstSlash + 1);
      if (!vtText.empty())
      {
        return ParseObjInt(vtText, out.vt);
      }
      return true;
    }

    const std::string vtText =
        token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
    const std::string vnText = token.substr(secondSlash + 1);
    if (!vtText.empty() && !ParseObjInt(vtText, out.vt))
    {
      return false;
    }
    if (!vnText.empty() && !ParseObjInt(vnText, out.vn))
    {
      return false;
    }
    return true;
  }

  static int ResolveObjIndex(int objIndex, size_t count)
  {
    if (objIndex > 0)
    {
      return objIndex - 1;
    }
    if (objIndex < 0)
    {
      return static_cast<int>(count) + objIndex;
    }
    return -1;
  }

  static bool AppendObjVertex(
      const ObjVertexKey &key, const std::vector<Vector3> &rawPositions,
      const std::vector<Vector2> &rawUVs, const std::vector<Vector3> &rawNormals,
      std::unordered_map<ObjVertexKey, uint32_t, ObjVertexKeyHasher> &vertexMap,
      Mesh &mesh)
  {
    const auto found = vertexMap.find(key);
    if (found != vertexMap.end())
    {
      mesh.indices.push_back(found->second);
      return true;
    }

    const int positionIndex = ResolveObjIndex(key.v, rawPositions.size());
    if (positionIndex < 0 ||
        positionIndex >= static_cast<int>(rawPositions.size()))
    {
      return false;
    }

    const uint32_t newIndex = static_cast<uint32_t>(mesh.verts.size());
    mesh.verts.push_back(rawPositions[positionIndex]);

    Vector2 uv{0.0f, 0.0f};
    if (key.vt != 0)
    {
      const int uvIndex = ResolveObjIndex(key.vt, rawUVs.size());
      if (uvIndex >= 0 && uvIndex < static_cast<int>(rawUVs.size()))
      {
        uv = rawUVs[uvIndex];
        mesh.hasUVs = true;
      }
    }
    mesh.uvs.push_back(uv);

    Vector3 normal{0.0f, 0.0f, 0.0f};
    if (key.vn != 0)
    {
      const int normalIndex = ResolveObjIndex(key.vn, rawNormals.size());
      if (normalIndex >= 0 && normalIndex < static_cast<int>(rawNormals.size()))
      {
        normal = rawNormals[normalIndex];
      }
    }
    mesh.normals.push_back(normal);

    vertexMap.emplace(key, newIndex);
    mesh.indices.push_back(newIndex);
    return true;
  }

  static void GenerateNormals(Mesh &mesh)
  {
    mesh.normals.assign(mesh.verts.size(), {0.0f, 0.0f, 0.0f});

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
      const uint32_t i0 = mesh.indices[i];
      const uint32_t i1 = mesh.indices[i + 1];
      const uint32_t i2 = mesh.indices[i + 2];
      const Vector3 &v0 = mesh.verts[i0];
      const Vector3 &v1 = mesh.verts[i1];
      const Vector3 &v2 = mesh.verts[i2];

      const Vector3 faceNormal =
          math::CrossProduct(v1 - v0, v2 - v0);
      if (math::DotProduct(faceNormal, faceNormal) <= 1e-8f)
      {
        continue;
      }

      mesh.normals[i0] = mesh.normals[i0] + faceNormal;
      mesh.normals[i1] = mesh.normals[i1] + faceNormal;
      mesh.normals[i2] = mesh.normals[i2] + faceNormal;
    }

    for (Vector3 &normal : mesh.normals)
    {
      if (math::DotProduct(normal, normal) <= 1e-8f)
      {
        normal = {0.0f, 1.0f, 0.0f};
        continue;
      }
      normal = normal.Normalize();
    }

    mesh.hasNormals = true;
  }

  std::string m_ResourceDirectorySuffix;
  std::vector<TGA *> m_LoadedTga;
  uint32_t m_SupportedTextureFormat;
};
