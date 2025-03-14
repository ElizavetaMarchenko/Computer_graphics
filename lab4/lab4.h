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
    float u, v;    // Текстурные координаты
};

struct SphereVertex {
    float x, y, z;
};

static const TextureVertex Vertices[24] = {
    {-0.5f, -0.5f,  0.5f, 0.0f, 1.0f}, // 0
    { 0.5f, -0.5f,  0.5f, 1.0f, 1.0f}, // 1
    { 0.5f, -0.5f, -0.5f, 1.0f, 0.0f}, // 2
    {-0.5f, -0.5f, -0.5f, 0.0f, 0.0f}, // 3

    { 0.5f, -0.5f, -0.5f, 0.0f, 1.0f}, // 4
    { 0.5f, -0.5f,  0.5f, 1.0f, 1.0f}, // 5
    { 0.5f,  0.5f,  0.5f, 1.0f, 0.0f}, // 6
    { 0.5f,  0.5f, -0.5f, 0.0f, 0.0f}, // 7

    {-0.5f, -0.5f,  0.5f, 0.0f, 1.0f}, // 8
    { 0.5f, -0.5f,  0.5f, 1.0f, 1.0f}, // 9
    { 0.5f,  0.5f,  0.5f, 1.0f, 0.0f}, // 10
    {-0.5f,  0.5f,  0.5f, 0.0f, 0.0f}, // 11

    {-0.5f, -0.5f, -0.5f, 1.0f, 1.0f}, // 12
    { 0.5f, -0.5f, -0.5f, 0.0f, 1.0f}, // 13
    { 0.5f,  0.5f, -0.5f, 0.0f, 0.0f}, // 14
    {-0.5f,  0.5f, -0.5f, 1.0f, 0.0f}, // 15

    {-0.5f,  0.5f,  0.5f, 0.0f, 1.0f}, // 16
    { 0.5f,  0.5f,  0.5f, 1.0f, 1.0f}, // 17
    { 0.5f,  0.5f, -0.5f, 1.0f, 0.0f}, // 18
    {-0.5f,  0.5f, -0.5f, 0.0f, 0.0f}, // 19

    {-0.5f, -0.5f, -0.5f, 0.0f, 1.0f}, // 20
    {-0.5f, -0.5f,  0.5f, 1.0f, 1.0f}, // 21
    {-0.5f,  0.5f,  0.5f, 1.0f, 0.0f}, // 22
    {-0.5f,  0.5f, -0.5f, 0.0f, 0.0f}  // 23
};

struct GeomBuffer {
    DirectX::XMMATRIX model;
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX projection;
};

struct SceneBuffer {
    DirectX::XMMATRIX vp;
    DirectX::XMFLOAT4 cameraPos;
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

const char* vertexShaderCode = R"(
cbuffer GeomBuffer : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};

struct VSInput
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;
    float4 pos = float4(vertex.pos, 1.0);
    pos = mul(model, pos);
    pos = mul(view, pos);
    pos = mul(projection, pos);
    result.pos = pos;
    result.uv = vertex.uv;
    return result;
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

const char* pixelShaderCode = R"(
Texture2D colorTexture : register(t0);
SamplerState colorSampler : register(s0);

struct VSOutput {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

float4 ps(VSOutput pixel) : SV_Target {
    return colorTexture.Sample(colorSampler, pixel.uv);
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
    return float4(colorTexture.Sample(colorSampler, pixel.localPos).xyz, 1.0);
}
)";



