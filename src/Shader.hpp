#pragma once

#include <algorithm>

#include "Color.hpp"
#include "Math.hpp"
#include "TGA.hpp"

struct VertexInput {
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
};

struct VertexOutput {
    Vector4 clipPosition;
    Vector3 worldPosition;
    Vector3 normal;
    Vector2 uv;
    float invW { 1.0f };
};

struct PixelInput {
    Vector3 worldPosition;
    Vector3 normal;
    Vector2 uv;
};

struct ShaderUniforms {
    Matrix4x4 model;
    Matrix4x4 view;
    Matrix4x4 projection;
    
    Vector3 lightDir;
    Vector3 cameraPosition;
    
    float ambientStrength;
    float diffuseStrength;
    float specularStrength;
    float shininess;
    
    Color specularColor;
    const TGA* texture { nullptr };
};

class Shader {
public:
    virtual ~Shader() = default;

    /**
     * The vertex shader transforms a vertex from model space to clip space and
     * passes through any additional data needed for the pixel shader.
     * 
     * Transform local position to world position
     * Transform to clip space with projection * view * model
     * Transform normal to world space
     * Pass uv
     * Store invW = 1.0f / clipPosition.w for perspective-correct interpolation later
     */
    virtual VertexOutput VertexShader(const VertexInput& input, const ShaderUniforms& uniforms) const = 0;

    /**
     * The pixel shader calculates the final color of a pixel based on the input
     * from the vertex shader and the shader uniforms.
     * 
     * Normalize interpolated normal
     * Compute diffuse light with max(dot(normal, lightDir), 0.0f)
     * Optionally sample texture using interpolated uv
     * Combine texture/base color with lighting
     * Return final color
     */
    virtual Color PixelShader(const PixelInput& input, const ShaderUniforms& uniforms) const = 0;
};

class BasicLitShader : public Shader {
public:
    VertexOutput VertexShader(const VertexInput& input, const ShaderUniforms& uniforms) const override {
        VertexOutput output;

        // Transform position to world space
        Vector4 worldPos = uniforms.model * Vector4(input.position.x, input.position.y, input.position.z, 1.0f);
        output.worldPosition = Vector3(worldPos.x, worldPos.y, worldPos.z);

        // Transform to clip space
        output.clipPosition = uniforms.projection * (uniforms.view * worldPos);

        // Transform normal to world space (ignore translation)
        Vector4 worldNormal = uniforms.model * Vector4(input.normal.x, input.normal.y, input.normal.z, 0.0f);
        output.normal = Vector3(worldNormal.x, worldNormal.y, worldNormal.z).Normalize();

        // Pass through UV
        output.uv = input.uv;

        // Store inverse W for perspective-correct interpolation
        output.invW =
            (std::abs(output.clipPosition.w) > 1e-6f) ? (1.0f / output.clipPosition.w) : 0.0f;

        return output;
    }

    Color PixelShader(const PixelInput& input, const ShaderUniforms& uniforms) const override {
        // Compute diffuse lighting
        float diffuse = std::max(0.0f, math::DotProduct(input.normal.Normalize(), uniforms.lightDir.Normalize()));
        float light = 0.2f + diffuse * 0.8f;

        // Sample texture if available
        Color baseColor = Color(0xFFFFFFFF); // default white
        if (uniforms.texture != nullptr) {
            baseColor = uniforms.texture->Sample(input.uv.x, input.uv.y);
        }

        // Combine base color with lighting
        Color finalColor;
        finalColor.r = static_cast<uint8_t>(baseColor.r * light);
        finalColor.g = static_cast<uint8_t>(baseColor.g * light);
        finalColor.b = static_cast<uint8_t>(baseColor.b * light);
        finalColor.a = baseColor.a;
        return finalColor;
    }
};

class PhongShader : public BasicLitShader {
public:
    VertexOutput VertexShader(const VertexInput& input, const ShaderUniforms& uniforms) const override {
        return BasicLitShader::VertexShader(input, uniforms);
    }

    Color PixelShader(const PixelInput& input, const ShaderUniforms& uniforms) const override {
        // Consider the light and apply the light each verts not on the surface of triangle
        const Vector3 n = input.normal.Normalize();
        const Vector3 l = uniforms.lightDir.Normalize();
        const Vector3 v = (uniforms.cameraPosition - input.worldPosition).Normalize();
        
        // R = normalize(2 * dot(N, L) * N - L)
        const float ndotl = std::max(0.0f, math::DotProduct(n, l));
        const Vector3 r = (n * 2 * math::DotProduct(n, l) - l).Normalize();

        // ambient + kd * max(dot(N, L), 0) + ks * pow(max(dot(R, V), 0), shininess)
        const float diffuse = uniforms.diffuseStrength * ndotl;

        const float specular = (ndotl > 0.0f) ?
            uniforms.specularStrength * std::pow(std::max(0.0f, math::DotProduct(r, v)), uniforms.shininess) : 0.0f;
        
        Color texColor = (uniforms.texture != nullptr) ?
            uniforms.texture->Sample(input.uv.x, input.uv.y) : Color(0xFFFFFFFF);
        Vector3 albedo = { texColor.r / 255.0f, texColor.g / 255.0f, texColor.b / 255.0f };
        Vector3 specColor = { uniforms.specularColor.r / 255.0f,
            uniforms.specularColor.g / 255.0f, uniforms.specularColor.b / 255.0f };
        
        Vector3 floatColor = albedo * (uniforms.ambientStrength + diffuse) + specColor * specular;

        Color finalColor;
        finalColor.r = static_cast<uint8_t>(std::clamp(floatColor.x, 0.0f, 1.0f) * 255.0f);
        finalColor.g = static_cast<uint8_t>(std::clamp(floatColor.y, 0.0f, 1.0f) * 255.0f);
        finalColor.b = static_cast<uint8_t>(std::clamp(floatColor.z, 0.0f, 1.0f) * 255.0f);
        finalColor.a = texColor.a;
        return finalColor;
    }
};

class BlinnPhongShader : public BasicLitShader {
public:
    VertexOutput VertexShader(const VertexInput& input, const ShaderUniforms& uniforms) const override {
        return BasicLitShader::VertexShader(input, uniforms);
    }
    
    Color PixelShader(const PixelInput& input, const ShaderUniforms& uniforms) const override {
        const Vector3 n = input.normal.Normalize();
        const Vector3 l = uniforms.lightDir.Normalize();
        const Vector3 v = (uniforms.cameraPosition - input.worldPosition).Normalize();
        
        const float ndotl = std::max(0.0f, math::DotProduct(n, l));
        
        // Main difference from normal Phong shading
        const Vector3 h = (l + v).Normalize();
        
        const float diffuse = uniforms.diffuseStrength * ndotl;

        const float specular = (ndotl > 0.0f) ?
            uniforms.specularStrength * std::pow(std::max(0.0f, math::DotProduct(n, h)), uniforms.shininess)
            : 0.0f;
        
        Color texColor = (uniforms.texture != nullptr) ?
            uniforms.texture->Sample(input.uv.x, input.uv.y) : Color(0xFFFFFFFF);
        Vector3 albedo = { texColor.r / 255.0f, texColor.g / 255.0f, texColor.b / 255.0f };
        Vector3 specColor = { uniforms.specularColor.r / 255.0f,
            uniforms.specularColor.g / 255.0f, uniforms.specularColor.b / 255.0f };
        
        Vector3 floatColor = albedo * (uniforms.ambientStrength + diffuse) + specColor * specular;

        Color finalColor;
        finalColor.r = static_cast<uint8_t>(std::clamp(floatColor.x, 0.0f, 1.0f) * 255.0f);
        finalColor.g = static_cast<uint8_t>(std::clamp(floatColor.y, 0.0f, 1.0f) * 255.0f);
        finalColor.b = static_cast<uint8_t>(std::clamp(floatColor.z, 0.0f, 1.0f) * 255.0f);
        finalColor.a = texColor.a;
        return finalColor;
    }
};
