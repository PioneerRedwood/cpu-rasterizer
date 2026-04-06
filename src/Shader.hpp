#pragma once

#include <algorithm>

#include "Color.hpp"
#include "Math.hpp"
#include "TGA.hpp"
#include "Material.hpp"
#include "render/DepthTarget.hpp"

struct VertexInput
{
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
};

struct VertexOutput
{
    Vector4 clipPosition;
    Vector3 worldPosition;
    Vector3 normal;
    Vector2 uv;
    float invW{1.0f};
};

struct PixelInput
{
    Vector3 worldPosition;
    Vector3 normal;
    Vector2 uv;
};

struct ShaderUniforms
{
    Matrix4x4 model;
    Matrix4x4 view;
    Matrix4x4 projection;
    Matrix4x4 lightView;
    Matrix4x4 lightProjection;
    Matrix4x4 shadowViewport;

    Vector3 lightDir;
    Vector3 cameraPosition;

    float ambientStrength;
    float diffuseStrength;
    float specularStrength;
    float shininess;
    float shadowBias{0.003f};

    Color specularColor;
    const TGA *texture{nullptr};
    const render::DepthTarget *shadowMap{nullptr};
};

struct PBRShaderUniforms : public ShaderUniforms
{
    // PBR-specific uniforms can be added here if needed
    const Material *material{nullptr};
    Color lightColor;
    Vector3 directionalLight;
};

inline float ComputeShadowVisibility(const PixelInput &input,
                                     const ShaderUniforms &uniforms)
{
    if (uniforms.shadowMap == nullptr || uniforms.shadowMap->data.empty())
    {
        return 1.0f;
    }

    Vector4 worldPos(input.worldPosition.x, input.worldPosition.y,
                     input.worldPosition.z, 1.0f);
    Vector4 lightClip =
        uniforms.lightProjection * (uniforms.lightView * worldPos);
    if (lightClip.w <= 1e-6f)
    {
        return 1.0f;
    }

    lightClip.PerspectiveDivide();
    if (!std::isfinite(lightClip.x) || !std::isfinite(lightClip.y) ||
        !std::isfinite(lightClip.z))
    {
        return 1.0f;
    }

    if (lightClip.x < -1.0f || lightClip.x > 1.0f ||
        lightClip.y < -1.0f || lightClip.y > 1.0f ||
        lightClip.z < 0.0f || lightClip.z > 1.0f)
    {
        return 1.0f;
    }

    const Vector4 shadowScreen = uniforms.shadowViewport * lightClip;
    const int sx = std::clamp(static_cast<int>(shadowScreen.x), 0,
                              uniforms.shadowMap->width - 1);
    const int sy = std::clamp(static_cast<int>(shadowScreen.y), 0,
                              uniforms.shadowMap->height - 1);
    const float closestDepth =
        uniforms.shadowMap->data[sx + sy * uniforms.shadowMap->width];
    if (closestDepth >= 1.0f)
    {
        return 1.0f;
    }

    const float ndotl = std::max(
        0.0f,
        math::DotProduct(input.normal.Normalize(), uniforms.lightDir.Normalize()));
    const float bias =
        std::max(uniforms.shadowBias * (1.0f - ndotl),
                 uniforms.shadowBias * 0.25f);

    return (lightClip.z - bias > closestDepth) ? 0.0f : 1.0f;
}

class Shader
{
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
    virtual VertexOutput VertexShader(const VertexInput &input, const ShaderUniforms &uniforms) const = 0;

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
    virtual Color PixelShader(const PixelInput &input, const ShaderUniforms &uniforms) const = 0;
};

class BasicLitShader : public Shader
{
public:
    VertexOutput VertexShader(const VertexInput &input, const ShaderUniforms &uniforms) const override
    {
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

    Color PixelShader(const PixelInput &input, const ShaderUniforms &uniforms) const override
    {
        // Compute diffuse lighting
        float diffuse = std::max(0.0f, math::DotProduct(input.normal.Normalize(), uniforms.lightDir.Normalize()));
        const float shadowVisibility = ComputeShadowVisibility(input, uniforms);
        float light = 0.2f + diffuse * 0.8f * shadowVisibility;

        // Sample texture if available
        Color baseColor = Color(0xFFFFFFFF); // default white
        if (uniforms.texture != nullptr)
        {
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

class PhongShader : public BasicLitShader
{
public:
    VertexOutput VertexShader(const VertexInput &input, const ShaderUniforms &uniforms) const override
    {
        return BasicLitShader::VertexShader(input, uniforms);
    }

    Color PixelShader(const PixelInput &input, const ShaderUniforms &uniforms) const override
    {
        // Consider the light and apply the light each verts not on the surface of triangle
        const Vector3 n = input.normal.Normalize();
        const Vector3 l = uniforms.lightDir.Normalize();
        const Vector3 v = (uniforms.cameraPosition - input.worldPosition).Normalize();
        const float shadowVisibility = ComputeShadowVisibility(input, uniforms);

        // R = normalize(2 * dot(N, L) * N - L)
        const float ndotl = std::max(0.0f, math::DotProduct(n, l));
        const Vector3 r = (n * 2 * math::DotProduct(n, l) - l).Normalize();

        // ambient + kd * max(dot(N, L), 0) + ks * pow(max(dot(R, V), 0), shininess)
        const float diffuse = uniforms.diffuseStrength * ndotl;

        const float specular = (ndotl > 0.0f) ? uniforms.specularStrength * std::pow(std::max(0.0f, math::DotProduct(r, v)), uniforms.shininess) : 0.0f;

        Color texColor = (uniforms.texture != nullptr) ? uniforms.texture->Sample(input.uv.x, input.uv.y) : Color(0xFFFFFFFF);
        Vector3 albedo = {texColor.r / 255.0f, texColor.g / 255.0f, texColor.b / 255.0f};
        Vector3 specColor = {uniforms.specularColor.r / 255.0f,
                             uniforms.specularColor.g / 255.0f, uniforms.specularColor.b / 255.0f};

        Vector3 floatColor = albedo * uniforms.ambientStrength +
                             (albedo * diffuse + specColor * specular) * shadowVisibility;

        Color finalColor;
        finalColor.r = static_cast<uint8_t>(std::clamp(floatColor.x, 0.0f, 1.0f) * 255.0f);
        finalColor.g = static_cast<uint8_t>(std::clamp(floatColor.y, 0.0f, 1.0f) * 255.0f);
        finalColor.b = static_cast<uint8_t>(std::clamp(floatColor.z, 0.0f, 1.0f) * 255.0f);
        finalColor.a = texColor.a;
        return finalColor;
    }
};

class BlinnPhongShader : public BasicLitShader
{
public:
    VertexOutput VertexShader(const VertexInput &input, const ShaderUniforms &uniforms) const override
    {
        return BasicLitShader::VertexShader(input, uniforms);
    }

    Color PixelShader(const PixelInput &input, const ShaderUniforms &uniforms) const override
    {
        const Vector3 n = input.normal.Normalize();
        const Vector3 l = uniforms.lightDir.Normalize();
        const Vector3 v = (uniforms.cameraPosition - input.worldPosition).Normalize();
        const float shadowVisibility = ComputeShadowVisibility(input, uniforms);

        const float ndotl = std::max(0.0f, math::DotProduct(n, l));

        // Main difference from normal Phong shading
        const Vector3 h = (l + v).Normalize();

        const float diffuse = uniforms.diffuseStrength * ndotl;

        const float specular = (ndotl > 0.0f) ? uniforms.specularStrength * std::pow(std::max(0.0f, math::DotProduct(n, h)), uniforms.shininess)
                                              : 0.0f;

        Color texColor = (uniforms.texture != nullptr) ? uniforms.texture->Sample(input.uv.x, input.uv.y) : Color(0xFFFFFFFF);
        Vector3 albedo = {texColor.r / 255.0f, texColor.g / 255.0f, texColor.b / 255.0f};
        Vector3 specColor = {uniforms.specularColor.r / 255.0f,
                             uniforms.specularColor.g / 255.0f, uniforms.specularColor.b / 255.0f};

        Vector3 floatColor = albedo * uniforms.ambientStrength +
                             (albedo * diffuse + specColor * specular) * shadowVisibility;

        Color finalColor;
        finalColor.r = static_cast<uint8_t>(std::clamp(floatColor.x, 0.0f, 1.0f) * 255.0f);
        finalColor.g = static_cast<uint8_t>(std::clamp(floatColor.y, 0.0f, 1.0f) * 255.0f);
        finalColor.b = static_cast<uint8_t>(std::clamp(floatColor.z, 0.0f, 1.0f) * 255.0f);
        finalColor.a = texColor.a;
        return finalColor;
    }
};

const float PI = 3.141592653;

class PBRShader : public BasicLitShader
{
public:
    VertexOutput VertexShader(const VertexInput &input, const ShaderUniforms &uniforms) const override
    {
        return BasicLitShader::VertexShader(input, uniforms);
    }

    Color PixelShader(const PixelInput &input, const ShaderUniforms &shaderUniforms) const override
    {
        const PBRShaderUniforms &uniforms = static_cast<const PBRShaderUniforms &>(shaderUniforms);

        const Vector3 N = input.normal.Normalize();
        const Vector3 V = (uniforms.cameraPosition - input.worldPosition).Normalize();

        const Material &mat = *uniforms.material;
        // 비금속성의 기본 반사율은 약 0.04
        // 금속은 albedo 색상 자체가 반사율이 됨
        Vector3 F0 = math::Lerp({0.04f}, mat.albedo, mat.metallic);

        // 광원 방향 (위치 -> 방향 벡터)
        const Vector3 L = (uniforms.directionalLight - input.worldPosition).Normalize();
        const Vector3 H = (V + L).Normalize(); // view + light 방향 벡터 Half

        // 감쇠되어 표면에 들어온 빛값
        Vector3 radiance = Vector3{
            static_cast<float>(uniforms.lightColor.r / 255.0f),
            static_cast<float>(uniforms.lightColor.g / 255.0f),
            static_cast<float>(uniforms.lightColor.b / 255.0f)};

        // Cook-torrance BRDF
        float NDF = DistributionGGX(N, H, mat.roughness);                       // Normal Distribution Function
        float G = GeometrySmith(N, V, L, mat.roughness);                        // Geometry Function
        Vector3 F = FresnelSchlick(std::max(math::DotProduct(H, V), 0.0f), F0); // Fresnel-Schlick approximation

        // 에너지 보존
        Vector3 kS = F;
        // 금속성이 아니면 diffuse 없음, 금속성은 모든 빛을 반사시키므로 specualar == diffuse
        Vector3 kD = Vector3(1.0f, 1.0f, 1.0f) - kS * (1.0f - mat.metallic);

        // Specular (정반사)
        Vector3 numerator = F * NDF * G;
        float denominator = 4.0f * std::max(math::DotProduct(N, V), 0.0f) * std::max(math::DotProduct(N, L), 0.0f);
        Vector3 specular = numerator / std::max(denominator, 0.001f); // 분모가 0이 되는 것을 방지

        // 반사율 방정식 합산
        float NdotL = std::max(math::DotProduct(N, L), 0.0f);

        // 직접 광이므로 그림자 영향 받음
        const float shadowVisibility = ComputeShadowVisibility(input, uniforms);
        Vector3 Lo = (kD * mat.albedo / PI + specular) * radiance * NdotL * shadowVisibility;

        // Ambient (나중에 IBL 교채 가능)
        Vector3 ambient = Vector3(0.03f) * mat.albedo * mat.ambientOcclusion;
        Vector3 color = ambient + Lo;

        // tone mapping HDR->LDR
        color = ACESFilmic(color);

        // gamma correction
        color = math::Pow(color, Vector3(1.0f / 2.2));

        Color finalColor;
        finalColor.r = static_cast<uint8_t>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f);
        finalColor.g = static_cast<uint8_t>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f);
        finalColor.b = static_cast<uint8_t>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f);
        finalColor.a = 255;
        return finalColor;
    }

private:
    // ACES filmic, 하이라이트 색상 보존이 훨씬 좋음
    Vector3 ACESFilmic(Vector3 x) const
    {
        const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
        return (x * (x * a + Vector3(b))) / (x * (x * c + Vector3(d)) + Vector3(e));
    }

    // DistributionGGX: 미세표면 중 Half 벡터 방향을 향한 비율
    // roughness 가 낮을수록 하이라이트가 좁고 강하고, 높을수록 넓고 흐림
    float DistributionGGX(Vector3 N, Vector3 H, float roughness) const
    {
        float a = roughness * roughness; // Perceptual roughness -> alpha
        float a2 = a * a;
        float NdotH = std::max(math::DotProduct(N, H), 0.0f);
        float NdotH2 = NdotH * NdotH;

        float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
        denom = PI * denom * denom; // π * (NdotH²(α²-1)+1)²

        return a2 / std::max(denom, 0.0001f); // 분모 0 방지
    }

    // GeometrySchlickGGX: 단방향(시선 or 광원) 가림 계산
    // 시선이나 광원이 표면에 비스듬할수록 미세면에 가려지는 비율 증가
    float GeometrySchlickGGX(float NdotX, float roughness) const
    {
        float r = roughness + 1.0f;
        float k = (r * r) / 8.0f; // Direct lighting 용 k (IBL이면 k = a²/2)
        return NdotX / (NdotX * (1.0f - k) + k);
    }

    // GeometrySmith: 시선 방향 + 광원 방향 양쪽 가림을 모두 곱함
    float GeometrySmith(Vector3 N, Vector3 V, Vector3 L, float roughness) const
    {
        float NdotV = std::max(math::DotProduct(N, V), 0.0f);
        float NdotL = std::max(math::DotProduct(N, L), 0.0f);
        float ggxV = GeometrySchlickGGX(NdotV, roughness); // 시선 방향 가림
        float ggxL = GeometrySchlickGGX(NdotL, roughness); // 광원 방향 가림
        return ggxV * ggxL;
    }

    // FresenelSchlick: 보는 각도가 비스듬할수록 (cosTheta -> 0) 반사율이 1에 가까워짐
    // cosTheta = dot(H, V), F0 = 정면에서의 기본 반사율
    Vector3 FresnelSchlick(float cosTheta, Vector3 F0) const
    {
        float x = 1.0f - cosTheta;
        float x5 = x * x * x * x * x; // (1 - cosTheta)^5 - pow() 대신 직접 계산 (성능)
        return F0 + (Vector3(1.0f) - F0) * x5;
    }
};