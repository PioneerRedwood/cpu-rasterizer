#pragma once

#include "FileSystem.hpp"
#include "Log.hpp"
#include "Mesh.hpp"
#include "TGA.hpp"
#include "Renderable.hpp"

#include <SDL.h>

class ResourceLoader
{
public:
    explicit ResourceLoader(SDL_Renderer &renderer)
        : m_RendererRef(renderer)
    {
        m_ResourceDirectoryPrefix = std::string(CPURASTERIZER_RESOURCE_DIR);
    }

    ~ResourceLoader()
    {
        for (Mesh *mesh : m_LoadedMesh)
        {
            delete mesh;
        }
        for (TGA *tga : m_LoadedTga)
        {
            delete tga;
        }
        for (Renderable *Renderable : m_LoadedRenderables)
        {
            delete Renderable;
        }
    }

    Mesh *LoadMesh(const char *filepath)
    {
        if (filepath == nullptr)
        {
            return nullptr;
        }

        LogF("[ResourceLoader] Attempting to load mesh from %s", filepath);

        namespace fs = std::filesystem;
        const fs::path meshPath(m_ResourceDirectoryPrefix + "/" + filepath);

        FILE *fp = fopen(meshPath.string().c_str(), "r");
        if (fp == nullptr)
        {
            LogF("[ResourceLoader] Failed to open %s", meshPath.string().c_str());
            return nullptr;
        }

        Mesh *mesh = new Mesh();
        using Vec3 = std::vector<Vector3>;
        using Vec2 = std::vector<Vector2>;
        Vec3 rawPos;
        Vec2 rawUv;
        Vec3 rawNorm;

        auto AddVertex = [](Mesh *mesh, const Vec3 &pos, const Vec2 &uv, const Vec3 &norms,
                            int vi, int ti, int ni)
        {
            mesh->verts.push_back(pos[vi - 1]);
            mesh->uvs.push_back(ti > 0 ? uv[ti - 1] : Vector2(0.0f, 0.0f));
            mesh->normals.push_back(ni > 0 ? norms[ni - 1] : Vector3(0.0f, 0.0f, 0.0f));
            mesh->indices.push_back((uint32_t)mesh->verts.size() - 1);
        };

        int reserveHint = 0;
        char line[256];
        while (fgets(line, sizeof(line), fp))
        {
            if (isdigit((unsigned char)line[0]))
            {
                reserveHint = (int)std::strtol(line, nullptr, 10);
            }

            if (!strncmp(line, "v ", 2))
            {
                Vector3 v;
                if (sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z))
                {
                    if (reserveHint > 0)
                    {
                        rawPos.reserve(reserveHint);
                        reserveHint = 0;
                    }
                    rawPos.push_back(v);
                }
            }
            else if (!strncmp(line, "vt ", 3))
            {
                Vector2 uv;
                if (sscanf(line + 3, "%f %f", &uv.x, &uv.y))
                {
                    if (reserveHint > 0)
                    {
                        rawUv.reserve(reserveHint);
                        reserveHint = 0;
                    }
                    rawUv.push_back(uv);
                }
            }
            else if (!strncmp(line, "vn ", 3))
            {
                Vector3 n;
                if (sscanf(line + 3, "%f %f %f", &n.x, &n.y, &n.z))
                {
                    if (reserveHint > 0)
                    {
                        rawNorm.reserve(reserveHint);
                        reserveHint = 0;
                    }
                    rawNorm.push_back(n);
                }
            }
            else if (!strncmp(line, "f ", 2))
            {
                // face parse
                int v[3] = {}, t[3] = {}, n[3] = {};
                bool parsed = false;
                if (sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d",
                           &v[0], &t[0], &n[0],
                           &v[1], &t[1], &n[1],
                           &v[2], &t[2], &n[2]) == 9)
                {
                    parsed = true;
                }
                else if (sscanf(line, "f %d//%d %d//%d %d//%d",
                                &v[0], &n[0],
                                &v[1], &n[1],
                                &v[2], &n[2]) == 6)
                {
                    parsed = true;
                }
                else if (sscanf(line, "f %d/%d %d/%d %d/%d",
                                &v[0], &t[0],
                                &v[1], &t[1],
                                &v[2], &t[2]) == 6)
                {
                    parsed = true;
                }
                else if (sscanf(line, "f %d %d %d",
                                &v[0], &v[1], &v[2]) == 3)
                {
                    parsed = true;
                }

                if (!parsed)
                {
                    continue;
                }

                AddVertex(mesh, rawPos, rawUv, rawNorm, v[0], t[0], n[0]);
                AddVertex(mesh, rawPos, rawUv, rawNorm, v[1], t[1], n[1]);
                AddVertex(mesh, rawPos, rawUv, rawNorm, v[2], t[2], n[2]);
                mesh->hasUVs = mesh->hasUVs || (t[0] > 0 || t[1] > 0 || t[2] > 0);
                mesh->hasNormals = mesh->hasNormals || (n[0] > 0 || n[1] > 0 || n[2] > 0);
            }
            else
            {
                continue;
            }
        }

        m_LoadedMesh.push_back(mesh);
        return mesh;
    }

    uint32_t GetSupportedTextureFormat(uint32_t fallbackFormat)
    {
        // fallbackFormat "SDL_PIXELFORMAT_RGBA32"
        if (m_SupportedTextureFormat != SDL_PIXELFORMAT_UNKNOWN)
        {
            return m_SupportedTextureFormat;
        }

        SDL_RendererInfo info{};
        if (SDL_GetRendererInfo(&m_RendererRef, &info) != 0 ||
            info.num_texture_formats == 0)
        {
            m_SupportedTextureFormat = fallbackFormat;
            return m_SupportedTextureFormat;
        }

        LogF("[ResourceLoader] Renderer backend: %s", info.name);

        for (uint32_t i = 0; i < info.num_texture_formats; ++i)
        {
            const uint32_t format = info.texture_formats[i];
            if (!SDL_ISPIXELFORMAT_FOURCC(format) && SDL_BYTESPERPIXEL(format) == 4)
            {
                m_SupportedTextureFormat = format;
                return m_SupportedTextureFormat;
            }
        }

        m_SupportedTextureFormat = info.texture_formats[0];

        return m_SupportedTextureFormat;
    }

    TGA *LoadTGATextureWithName(const char *name,
                                uint32_t textureFormat = SDL_PIXELFORMAT_RGBA32)
    {
        if (name == nullptr || name[0] == '\0')
        {
            return nullptr;
        }
        const uint32_t supportedTextureFormat = GetSupportedTextureFormat(textureFormat);

        namespace fs = std::filesystem;
        const fs::path filename = m_ResourceDirectoryPrefix + "/" + name;

        if (!FileExist(filename))
        {
            LogF("[DEBUG] Create texture has failed : file does not exist!");
            return nullptr;
        }

        TGA *tga = new TGA();
        const std::string texturePath = filename.string();
        if (!tga->ReadFromFile(texturePath.c_str(), supportedTextureFormat))
        {
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

    Renderable *LoadRenderable(const char *meshFilepath)
    {
        Mesh *mesh = LoadMesh(meshFilepath);
        if (mesh == nullptr)
        {
            return nullptr;
        }

        Renderable *Renderable = new struct Renderable();
        Renderable->mesh = mesh;
        return Renderable;
    }

    Renderable *CreateGridPlaneMesh(int quadsX, int quadsZ, float sizeX,
                               float sizeZ)
    {
        Renderable *Renderable = new struct Renderable();
        Renderable->mesh = new Mesh();
        Renderable->mesh->verts.reserve((quadsX + 1) * (quadsZ + 1));
        Renderable->mesh->uvs.reserve((quadsX + 1) * (quadsZ + 1));
        Renderable->mesh->normals.reserve((quadsX + 1) * (quadsZ + 1));
        Renderable->mesh->indices.reserve(quadsX * quadsZ * 6);

        for (int z = 0; z <= quadsZ; ++z)
        {
            for (int x = 0; x <= quadsX; ++x)
            {
                const float u = static_cast<float>(x) / quadsX;
                const float v = static_cast<float>(z) / quadsZ;
                const float posX = (u - 0.5f) * sizeX;
                const float posZ = (v - 0.5f) * sizeZ;
                Renderable->mesh->verts.emplace_back(posX, 0.0f, posZ);
                Renderable->mesh->uvs.emplace_back(u, v);
                Renderable->mesh->normals.emplace_back(0.0f, 1.0f, 0.0f);
            }
        }

        for (int z = 0; z < quadsZ; ++z)
        {
            for (int x = 0; x < quadsX; ++x)
            {
                const uint32_t i0 = z * (quadsX + 1) + x;
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = i0 + (quadsX + 1);
                const uint32_t i3 = i2 + 1;

                Renderable->mesh->indices.push_back(i0);
                Renderable->mesh->indices.push_back(i2);
                Renderable->mesh->indices.push_back(i1);

                Renderable->mesh->indices.push_back(i1);
                Renderable->mesh->indices.push_back(i2);
                Renderable->mesh->indices.push_back(i3);
            }
        }

        Renderable->mesh->hasUVs = true;
        Renderable->mesh->hasNormals = true;
        m_LoadedRenderables.push_back(Renderable);
        return Renderable;
    }

private:
    SDL_Renderer &m_RendererRef;
    std::string m_ResourceDirectoryPrefix;

    std::vector<Mesh *> m_LoadedMesh;
    std::vector<TGA *> m_LoadedTga;
    std::vector<Renderable *> m_LoadedRenderables;
    uint32_t m_SupportedTextureFormat { SDL_PIXELFORMAT_UNKNOWN };
};
