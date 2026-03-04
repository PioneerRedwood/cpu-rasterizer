#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <SDL.h>
#include "RGBA.hpp"
#include "Log.hpp"
#include "TGA.hpp"

class TextureLoader
{
public:
    explicit TextureLoader(const char* suffix) {
        resourceDirectorySuffix = std::string(suffix);
    }

    ~TextureLoader() {
        for(auto* tga : loadedTga) {
            if(tga) {
                delete tga;
                tga = nullptr;
            }
        }
    }

    TGA* loadTGATextureWithName(SDL_Renderer* renderer, const char* name) {
        if (name == nullptr || name[0] == '\0') {
            return nullptr;
        }

        auto joinPath = [name](const std::string& dir) {
            std::string filename = dir;
            if (!filename.empty()) {
                const char c = filename.back();
                if (c != '/' && c != '\\') {
                    filename.push_back('/');
                }
            }
            filename.append(name);
            return filename;
        };

        const std::string candidates[] = {
            resourceDirectorySuffix,
            "resources",
            "../resources",
            "../../resources"
        };

        for (const std::string& dir : candidates) {
            if (dir.empty()) {
                continue;
            }

            const std::string filename = joinPath(dir);
            TGA* tga = new TGA();
            if (!tga->readFromFile(filename.c_str())) {
                delete tga;
                continue;
            }

            tga->createTexture(renderer);
            loadedTga.push_back(tga);
            return tga;
        }

        Logf("Failed to load texture file %s", name);
        return nullptr;
    }

private:
    std::string resourceDirectorySuffix;
    std::vector<TGA*> loadedTga;
};
