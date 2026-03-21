#pragma once

#include "Mesh.hpp"
#include "Log.hpp"
#include "FileSystem.hpp"

#include <SDL.h>

#include <vector>

class MeshLoader {
 public:
  explicit MeshLoader(SDL_Renderer& renderer)
      : m_RendererRef(renderer) {
    m_ResourceDirectoryPrefix = std::string(CPURASTERIZER_RESOURCE_DIR);
  }

  ~MeshLoader() {}

  Mesh* LoadMesh(const char* filepath) {
    if (filepath == nullptr) {
      return nullptr;
    }

    LogF("[DEBUG] Attempting to load mesh from %s", filepath);

    namespace fs = std::filesystem;
    const fs::path meshPath(m_ResourceDirectoryPrefix + "/" + filepath);

    FILE* fp = fopen(meshPath.string().c_str(), "r");
    if (fp == nullptr) {
      LogF("[DEBUG] Failed to open %s", meshPath.string().c_str());
      return nullptr;
    }

    Mesh* mesh = new Mesh();
    using Vec3 = std::vector<Vector3>;
    using Vec2 = std::vector<Vector2>;
    Vec3 rawPos;
    Vec2 rawUv;
    Vec3 rawNorm;

    auto AddVertex = [](Mesh* mesh, const Vec3& pos, const Vec2& uv, const Vec3& norms, 
      int vi, int ti, int ni) {
      mesh->verts.push_back(pos[vi - 1]);
      mesh->uvs.push_back(ti > 0 ? uv[ti - 1] : Vector2(0.0f, 0.0f));
      mesh->normals.push_back(ni > 0 ? norms[ni - 1] : Vector3(0.0f, 0.0f, 0.0f));
      mesh->indices.push_back((uint32_t) mesh->verts.size() - 1);
    };

    int reserveHint = 0;
    char line[256];
    while(fgets(line, sizeof(line), fp)) {
      if (isdigit((unsigned char)line[0])) {
        reserveHint = (int)std::strtol(line, nullptr, 10);
      }

      if(!strncmp(line, "v ", 2)) {
        Vector3 v;
        if(sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z)) {
          if(reserveHint > 0) { rawPos.reserve(reserveHint); reserveHint = 0; }
          rawPos.push_back(v);
        }
      } else if(!strncmp(line, "vt ", 3)) {
        Vector2 uv;
        if(sscanf(line + 3, "%f %f", &uv.x, &uv.y)) {
          if(reserveHint > 0) { rawUv.reserve(reserveHint); reserveHint = 0; }
          rawUv.push_back(uv);
        }
      } else if(!strncmp(line, "vn ", 3)) {
        Vector3 n;
        if(sscanf(line + 3, "%f %f %f", &n.x, &n.y, &n.z)) {
          if(reserveHint > 0) { rawNorm.reserve(reserveHint); reserveHint = 0; }
          rawNorm.push_back(n);
        }
      } else if(!strncmp(line, "f ", 2)) {
        // face parse
        int v[3] = {}, t[3] = {}, n[3] = {};
        bool parsed = false;
        if(sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d", 
                  &v[0], &t[0], &n[0],
                  &v[1], &t[1], &n[1],
                  &v[2], &t[2], &n[2]) == 9) {
          parsed = true;
        } else if (sscanf(line, "f %d//%d %d//%d %d//%d",
                          &v[0], &n[0],
                          &v[1], &n[1],
                          &v[2], &n[2]) == 6) {
          parsed = true;
        } else if (sscanf(line, "f %d/%d %d/%d %d/%d",
                          &v[0], &t[0],
                          &v[1], &t[1],
                          &v[2], &t[2]) == 6) {
          parsed = true;
        } else if (sscanf(line, "f %d %d %d",
                          &v[0], &v[1], &v[2]) == 3) {
          parsed = true;
        }

        if (!parsed) {
          continue;
        }

        AddVertex(mesh, rawPos, rawUv, rawNorm, v[0], t[0], n[0]);
        AddVertex(mesh, rawPos, rawUv, rawNorm, v[1], t[1], n[1]);
        AddVertex(mesh, rawPos, rawUv, rawNorm, v[2], t[2], n[2]);
        mesh->hasUVs = mesh->hasUVs || (t[0] > 0 || t[1] > 0 || t[2] > 0);
        mesh->hasNormals = mesh->hasNormals || (n[0] > 0 || n[1] > 0 || n[2] > 0);
      } else {
        continue;
      }
    }
    
      
    return mesh;
  }

  Mesh* LoadGLTF(const char* filepath) {}

 private:

  void GenerateNormals(Mesh& mesh) {
    mesh.normals.assign(mesh.verts.size(), {0.0f, 0.0f, 0.0f});

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
      const uint32_t i0 = mesh.indices[i];
      const uint32_t i1 = mesh.indices[i + 1];
      const uint32_t i2 = mesh.indices[i + 2];
      const Vector3& v0 = mesh.verts[i0];
      const Vector3& v1 = mesh.verts[i1];
      const Vector3& v2 = mesh.verts[i2];

      const Vector3 faceNormal = math::CrossProduct(v1 - v0, v2 - v0);
      if (math::DotProduct(faceNormal, faceNormal) <= 1e-8f) {
        continue;
      }

      mesh.normals[i0] = mesh.normals[i0] + faceNormal;
      mesh.normals[i1] = mesh.normals[i1] + faceNormal;
      mesh.normals[i2] = mesh.normals[i2] + faceNormal;
    }

    for (Vector3& normal : mesh.normals) {
      if (math::DotProduct(normal, normal) <= 1e-8f) {
        normal = {0.0f, 1.0f, 0.0f};
        continue;
      }
      normal = normal.Normalize();
    }

    mesh.hasNormals = true;
  }

  SDL_Renderer& m_RendererRef;
  std::string m_ResourceDirectoryPrefix;
};
