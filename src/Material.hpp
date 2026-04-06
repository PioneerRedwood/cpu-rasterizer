#pragma once

#include "Math.hpp"

struct Material
{
    Vector3 albedo;
    float metallic;
    float roughness;
    float ambientOcclusion;

    // 프리셋 팩토리 함수
    static Material Plastic()
    {
        // 비금속, 매끄러운 표면 - 흰 하이라이트
        return {{0.8f, 0.2f, 0.2f}, 0.0f, 0.3f, 1.0f};
    }

    static Material RoughPlastic()
    {
        // 비금속, 거친 표면 - 하이라이트가 넓고 흐림
        return {{0.8f, 0.2f, 0.2f}, 0.0f, 0.9f, 1.0f};
    }

    static Material Gold()
    {
        // 금속, albedo가 반사 색상이 됨 - 금색 하이라이트
        return {{1.0f, 0.766f, 0.336f}, 1.0f, 2.0f, 1.0f};
    }

    static Material Iron()
    {
        // 금속, 거친 표면
        return {{0.56f, 0.57f, 0.58f}, 1.0f, 0.6f, 1.0f};
    }

    static Material RubberBlack()
    {
        // 비금속, 매우 거침
        return {{0.05f, 0.05f, 0.05f}, 0.0f, 0.95f, 1.0f};
    }
};