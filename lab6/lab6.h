#pragma once

#include "resource.h"
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cmath>
#include <string>
#include <vector>
#include <chrono>
#include <DirectXMath.h>
#include "DirectXTex.h"
#include <algorithm>
#include <d3d11.h>
#include <windows.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct TextureVertex {
    float x, y, z; // Позиция вершины
    float u, v; // Текстурные координаты
};

struct TextureNormalVertex {
    float x, y, z; // Позиция вершины
    float nx, ny, nz; // Нормаль вершины
    float u, v; // Текстурные координаты
};

struct TextureTangentVertex {
    float x, y, z;       // Позиция вершины
    float nx, ny, nz;    // Нормаль вершины
    float tx, ty, tz;    // Касательный вектор
    float u, v;          // Текстурные координаты
};

struct SphereVertex {
    float x, y, z;
};

static const TextureTangentVertex Vertices[24] = {
    // Bottom face
    {-0.5f, -0.5f,  0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, // 0
    { 0.5f, -0.5f,  0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f}, // 1
    { 0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f}, // 2
    {-0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // 3

    // Right face
    { 0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f}, // 4
    { 0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f}, // 5
    { 0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f}, // 6
    { 0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f}, // 7

    // Front face
    {-0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, // 8
    { 0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f}, // 9
    { 0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f}, // 10
    {-0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // 11

    // Back face
    {-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f}, // 12
    { 0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, // 13
    { 0.5f,  0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // 14
    {-0.5f,  0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f}, // 15

    // Top face
    {-0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, // 16
    { 0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f}, // 17
    { 0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f}, // 18
    {-0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // 19

    // Left face
    {-0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f}, // 20
    {-0.5f, -0.5f,  0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f}, // 21
    {-0.5f,  0.5f,  0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f}, // 22
    {-0.5f,  0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f}  // 23
};

struct GeomBuffer {
    DirectX::XMMATRIX model;
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX projection;
    DirectX::XMMATRIX normalMatrix; // Матрица для преобразования нормалей
};

static const UINT16 Indices[36] = {
    0, 2, 1, 0, 3, 2,

    4, 6, 5, 4, 7, 6,

    8, 9, 10, 8, 10, 11,

    12, 14, 13, 12, 15, 14,

    16, 17, 18, 16, 18, 19,

    20, 21, 22, 20, 22, 23
};

struct TextureDesc
{
    UINT32 pitch = 0;
    UINT32 mipmapsCount = 0;
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    UINT32 width = 0;
    UINT32 height = 0;
    void* pData = nullptr;
    DirectX::ScratchImage image;
};

static const SphereVertex SkyboxVertices[24] = {
    {-100.0f, -100.0f,  100.0f}, // 0
    { 100.0f, -100.0f,  100.0f}, // 1
    { 100.0f, -100.0f, -100.0f}, // 2
    {-100.0f, -100.0f, -100.0f}, // 3

    { 100.0f, -100.0f, -100.0f}, // 4
    { 100.0f, -100.0f,  100.0f}, // 5
    { 100.0f,  100.0f,  100.0f}, // 6
    { 100.0f,  100.0f, -100.0f}, // 7

    {-100.0f, -100.0f,  100.0f}, // 8
    { 100.0f, -100.0f,  100.0f}, // 9
    { 100.0f,  100.0f,  100.0f}, // 10
    {-100.0f,  100.0f,  100.0f}, // 11

    {-100.0f, -100.0f, -100.0f}, // 12
    { 100.0f, -100.0f, -100.0f}, // 13
    { 100.0f,  100.0f, -100.0f}, // 14
    {-100.0f,  100.0f, -100.0f}, // 15

    {-100.0f,  100.0f,  100.0f}, // 16
    { 100.0f,  100.0f,  100.0f}, // 17
    { 100.0f,  100.0f, -100.0f}, // 18
    {-100.0f,  100.0f, -100.0f}, // 19

    {-100.0f, -100.0f, -100.0f}, // 20
    {-100.0f, -100.0f,  100.0f}, // 21
    {-100.0f,  100.0f,  100.0f}, // 22
    {-100.0f,  100.0f, -100.0f}  // 23
};

static const UINT16 SkyboxIndices[36] = {
    0, 1, 2, 0, 2, 3,

    4, 5, 6, 4, 6, 7,

    8, 10, 9, 8, 11, 10,

    12, 13, 14, 12, 14, 15,

    16, 18, 17, 16, 19, 18,

    20, 22, 21, 20, 23, 22
};

struct Light {
    DirectX::XMFLOAT4 pos; // Позиция источника света (x, y, z, w)
    DirectX::XMFLOAT4 color; // Цвет источника света (r, g, b, a)
};

struct SceneBuffer {
    DirectX::XMMATRIX vp; // Матрица вида и проекции
    DirectX::XMFLOAT4 cameraPos; // Позиция камеры (x, y, z, w)
    DirectX::XMFLOAT4 lightCount; // Количество источников света (x)
    Light lights[10]; // Массив источников света (максимум 10)
    DirectX::XMFLOAT4 ambientColor; // Цвет окружающего освещения (r, g, b, a)
};

struct MaterialBuffer {
    DirectX::XMFLOAT4 shine; // x - коэффициент блеска
};

const char* vertexShaderCode = R"(
cbuffer GeomBuffer : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    float4x4 normalMatrix; // Матрица для преобразования нормалей
};

struct VSInput
{
    float3 pos : POSITION;
    float3 norm : NORMAL;
    float3 tang : TANGENT;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float4 worldPos : POSITION;
    float3 norm : NORMAL;
    float3 tang : TANGENT;
    float2 uv : TEXCOORD;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;
    float4 pos = float4(vertex.pos, 1.0);
    result.worldPos = mul(model, pos); // Мировые координаты вершины
    pos = mul(view, result.worldPos);
    pos = mul(projection, pos);
    result.pos = pos;
    result.norm = mul((float3x3)normalMatrix, vertex.norm); // Преобразуем нормаль
    result.tang = mul((float3x3)model, vertex.tang);
    result.uv = vertex.uv;
    return result;
}
)";

const char* pixelShaderCode = R"(
Texture2D colorTexture : register(t0);
Texture2D normalMapTexture : register(t1);
SamplerState colorSampler : register(s0);

struct Light {
    float4 pos; // Позиция источника света (x, y, z, w)
    float4 color; // Цвет источника света (r, g, b, a)
};

cbuffer SceneBuffer : register(b1)
{
    float4x4 vp;
    float4 cameraPos;
    float4 lightCount; // x - количество источников света
    Light lights[10];
    float4 ambientColor;
};

cbuffer MaterialBuffer : register(b2)
{
    float4 shine; // x - коэффициент блеска
};

struct VSOutput
{
    float4 pos : SV_Position;
    float4 worldPos : POSITION;
    float3 norm : NORMAL;
    float3 tang : TANGENT;
    float2 uv : TEXCOORD;
};

float4 ps(VSOutput pixel) : SV_Target
{
    float3 color = colorTexture.Sample(colorSampler, pixel.uv).xyz;
    //return float4(color, 1.0);

    float3 finalColor = ambientColor.xyz * color; // Окружающее освещение

    // Нормаль из карты нормалей
    float3 normal = float3(0, 0, 0);
    float3 binorm = normalize(cross(pixel.norm, pixel.tang));
    float3 localNorm = normalMapTexture.Sample(colorSampler, pixel.uv).xyz * 2.0 - float3(1.0, 1.0, 1.0);
    normal = localNorm.x * normalize(pixel.tang) + localNorm.y * binorm + localNorm.z * normalize(pixel.norm);


    //float3 normal = normalize(pixel.norm);
    float3 viewDir = normalize(cameraPos.xyz - pixel.worldPos.xyz);

    for (int i = 0; i < lightCount.x; i++)
    {
        float3 lightDir = lights[i].pos.xyz - pixel.worldPos.xyz;
        float lightDist = length(lightDir);
        lightDir /= lightDist;

        // Диффузное освещение
        float diff = max(dot(lightDir, normal), 0.0);
        finalColor += color * diff * lights[i].color.xyz;

        // Зеркальное освещение
        float3 reflectDir = reflect(-lightDir, normal);
        float spec = shine.x > 0 ? pow(max(dot(viewDir, reflectDir), 0.0), shine.x) : 0.0;
        finalColor += color * spec * lights[i].color.xyz;
    }

    return float4(finalColor, 1.0);
}
)";

const char* vertexSphereShaderCode = R"(
cbuffer SceneBuffer : register(b0) {
    float4x4 vp;
    float4 cameraPos;
};
cbuffer GeomBuffer : register(b1) {
    float4x4 model;
};

struct VSInput {
    float3 pos : POSITION;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 localPos : POSITION1;
};

VSOutput vs(VSInput vertex) {
    VSOutput result;
    float3 pos = cameraPos.xyz + vertex.pos;
    result.pos = mul(vp, mul(model, float4(pos, 1.0)));
    result.localPos = vertex.pos;
    return result;
}
)";

const char* pixelSphereShaderCode = R"(
TextureCube colorTexture : register(t0);
SamplerState colorSampler : register(s0);

struct VSOutput {
    float4 pos : SV_Position;
    float3 localPos : POSITION1;
};

float4 ps(VSOutput pixel) : SV_Target0 {
    return float4(colorTexture.Sample(colorSampler, pixel.localPos).xyz * 0.5, 1.0);
}
)";

// полупрозрычные квадраты
struct SquareInfo {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
    float distance;
    UINT startIndex;
};

struct ColorBuffer {
    DirectX::XMFLOAT4 color;
};

static const TextureNormalVertex SquareVertices[] = {
    // Красный квадрат (смещение по X на -2)
    {0.9f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // 0
    {0.9f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f}, // 1
    {0.9f,  0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f}, // 2
    {0.9f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, // 3

    // Желтый квадрат (смещение по X на -3)
    {1.0f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // 4
    {1.0f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f}, // 5
    {1.0f,  0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f}, // 6
    {1.0f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}  // 7
};

static const UINT16 SquareIndices[] = {
    0, 1, 2, 0, 2, 3,
    4, 5, 6, 4, 6, 7
};

const char* vertexColorShaderCode = R"(
cbuffer GeomBuffer : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    float4x4 normalMatrix; // Матрица для преобразования нормалей
};

struct VSInput
{
    float3 pos : POSITION;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float4 worldPos : POSITION;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;
    float4 pos = float4(vertex.pos, 1.0);
    result.worldPos = mul(model, pos); // Мировые координаты вершины
    pos = mul(view, result.worldPos);
    pos = mul(projection, pos);
    result.pos = pos;
    result.norm = mul((float3x3)normalMatrix, vertex.norm); // Преобразуем нормаль
    result.uv = vertex.uv; // Передаем текстурные координаты
    return result;
}
)";

const char* pixelColorShaderCode = R"(
Texture2D colorTexture : register(t0);
SamplerState colorSampler : register(s0);

cbuffer ColorBuffer : register(b3) {
    float4 color; // Цвет квадрата
};
// Определение структуры Light
struct Light {
    float4 pos; // Позиция источника света (x, y, z, w)
    float4 color; // Цвет источника света (r, g, b, a)
};

cbuffer SceneBuffer : register(b1)
{
    float4x4 vp;
    float4 cameraPos;
    float4 lightCount; // x - количество источников света
    Light lights[10];
    float4 ambientColor;
};

cbuffer MaterialBuffer : register(b2)
{
    float4 shine; // x - коэффициент блеска
};

struct VSOutput
{
    float4 pos : SV_Position;
    float4 worldPos : POSITION;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
};

float4 ps(VSOutput pixel) : SV_Target
{
    float3 finalColor = ambientColor.xyz * color; // Окружающее освещение

    float3 normal = normalize(pixel.norm);
    float3 viewDir = normalize(cameraPos.xyz - pixel.worldPos.xyz);

    for (int i = 0; i < lightCount.x; i++)
    {
        float3 lightDir = lights[i].pos.xyz - pixel.worldPos.xyz;
        float lightDist = length(lightDir);
        lightDir /= lightDist;

        // Диффузное освещение
        //float diff = max(dot(lightDir, normal), 0.0);
        float diff = abs(dot(normal, lightDir));
        finalColor += color * diff * lights[i].color.xyz;

        // Зеркальное освещение
        float3 reflectDir = reflect(-lightDir, normal);
        float spec = shine.x > 0 ? pow(max(dot(viewDir, reflectDir), 0.0), shine.x) : 0.0;
        //finalColor += color * spec * lights[i].color.xyz;
    }

    return float4(finalColor, color.w);
}
)";


const char* pixelLightShaderCode = R"(
float4 ps() : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
)";
