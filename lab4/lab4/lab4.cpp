#include <d3d11.h>
#include "lab4.h"
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cmath>
#include <string>
#include <vector>
#include <chrono>
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <algorithm>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct TextureVertex {
    float x, y, z; // Позиция вершины
    float u, v;    // Текстурные координаты
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

HRESULT CreateVertexBuffer(ID3D11Device* pDevice, ID3D11Buffer** ppVertexBuffer) {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(Vertices);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = Vertices;

    return pDevice->CreateBuffer(&desc, &data, ppVertexBuffer);
}

HRESULT CreateIndexBuffer(ID3D11Device* pDevice, ID3D11Buffer** ppIndexBuffer) {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(Indices);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = Indices;

    return pDevice->CreateBuffer(&desc, &data, ppIndexBuffer);
}

HRESULT CompileShader(const char* shaderCode, const char* entryPoint, const char* target, ID3DBlob** ppCode) {
    ID3DBlob* pErrorBlob = nullptr;
    HRESULT hr = D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr, entryPoint, target, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, ppCode, &pErrorBlob);
    if (FAILED(hr)) {
        if (pErrorBlob) {
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        return hr;
    }
    if (pErrorBlob) pErrorBlob->Release();
    return S_OK;
}

HRESULT CreateInputLayout(ID3D11Device* pDevice, ID3D11InputLayout** ppInputLayout, ID3DBlob* pVertexShaderCode) {
    D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}, };
    return pDevice->CreateInputLayout(inputDesc, ARRAYSIZE(inputDesc), pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), ppInputLayout);
}

bool LoadDDS(const std::wstring& filePath, TextureDesc& textureDesc) {
    DirectX::ScratchImage image;
    HRESULT hr = DirectX::LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
    if (FAILED(hr)) {
        return false;
    }

    const DirectX::TexMetadata& metadata = image.GetMetadata();
    textureDesc.width = static_cast<UINT32>(metadata.width);
    textureDesc.height = static_cast<UINT32>(metadata.height);
    textureDesc.fmt = metadata.format;
    textureDesc.mipmapsCount = static_cast<UINT32>(metadata.mipLevels);
    textureDesc.pData = image.GetPixels();

    return true;
}

inline UINT32 DivUp(UINT32 value, UINT32 divisor) {
    return (value + divisor - 1) / divisor;
}

inline UINT32 GetBytesPerBlock(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        return 8;
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return 16;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
        return 4;
    default:
        return 0;
    }
}

HRESULT CreateTexture(ID3D11Device* pDevice, const TextureDesc& textureDesc, ID3D11Texture2D** ppTexture) { 
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = textureDesc.width;
    desc.Height = textureDesc.height;
    desc.MipLevels = textureDesc.mipmapsCount;
    desc.ArraySize = 1;
    desc.Format = textureDesc.fmt;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    UINT32 blockWidth = DivUp(desc.Width, 4u);
    UINT32 blockHeight = DivUp(desc.Height, 4u);
    UINT32 pitch = blockWidth * GetBytesPerBlock(desc.Format);

    const char* pSrcData = reinterpret_cast<const char*>(textureDesc.pData);
    std::vector<D3D11_SUBRESOURCE_DATA> data;
    data.resize(desc.MipLevels);

    for (UINT32 i = 0; i < desc.MipLevels; i++) {
        data[i].pSysMem = pSrcData;
        data[i].SysMemPitch = pitch;
        data[i].SysMemSlicePitch = 0;
        pSrcData += pitch * blockHeight;
        blockHeight = (blockHeight / 2) > 1 ? (blockHeight / 2) : 1;
        blockWidth = (blockWidth / 2) > 1 ? (blockWidth / 2) : 1;
        pitch = blockWidth * GetBytesPerBlock(desc.Format);
    }

    return pDevice->CreateTexture2D(&desc, data.data(), ppTexture);
}

HRESULT CreateShaderResourceView(ID3D11Device* pDevice, ID3D11Texture2D* pTexture, DXGI_FORMAT textureFmt, ID3D11ShaderResourceView** ppTextureView) {
    D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format = textureFmt;
    desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipLevels = -1; // Используем все уровни мипмап
    desc.Texture2D.MostDetailedMip = 0;

    return pDevice->CreateShaderResourceView(pTexture, &desc, ppTextureView);
}

HRESULT CreateSampler(ID3D11Device* pDevice, ID3D11SamplerState** ppSampler) {
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = D3D11_FILTER_ANISOTROPIC;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = FLT_MAX;
    desc.MipLODBias = 0.0f;
    desc.MaxAnisotropy = 16;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;

    return pDevice->CreateSamplerState(&desc, ppSampler);
}

void Render(ID3D11DeviceContext* pDeviceContext, ID3D11RenderTargetView* pRenderTargetView, ID3D11DepthStencilView* pDepthStencilView, ID3D11Buffer* pIndexBuffer, ID3D11Buffer* pVertexBuffer, ID3D11InputLayout* pInputLayout, ID3D11VertexShader* pVertexShader, ID3D11PixelShader* pPixelShader, ID3D11Buffer* pGeomBuffer, ID3D11SamplerState* pSampler, ID3D11ShaderResourceView* pTextureView) {
    static const FLOAT clearColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f }; // серый цвет
    pDeviceContext->ClearRenderTargetView(pRenderTargetView, clearColor);
    pDeviceContext->ClearDepthStencilView(pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    pDeviceContext->OMSetRenderTargets(1, &pRenderTargetView, pDepthStencilView);

    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = 1280;
    viewport.Height = 720;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    pDeviceContext->RSSetViewports(1, &viewport);

    pDeviceContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { pVertexBuffer };
    UINT strides[] = { sizeof(TextureVertex) };
    UINT offsets[] = { 0 };
    pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    pDeviceContext->IASetInputLayout(pInputLayout);
    pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pDeviceContext->VSSetShader(pVertexShader, nullptr, 0);
    pDeviceContext->PSSetShader(pPixelShader, nullptr, 0);
    pDeviceContext->VSSetConstantBuffers(0, 1, &pGeomBuffer);
    pDeviceContext->PSSetSamplers(0, 1, &pSampler);
    pDeviceContext->PSSetShaderResources(0, 1, &pTextureView);
    pDeviceContext->DrawIndexed(36, 0, 0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CLOSE:
        PostQuitMessage(0); // Завершаем цикл сообщений
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

HWND CreateWindowInstance(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"Lab4WindowClass";
    RegisterClassEx(&wcex);

    HWND hWnd = CreateWindow(wcex.lpszClassName, L"Lab4", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) {
        return nullptr;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return hWnd;
}

HRESULT InitDirectX(HWND hWnd, ID3D11Device** ppDevice, ID3D11DeviceContext** ppDeviceContext, IDXGISwapChain** ppSwapChain,
    ID3D11RenderTargetView** ppRenderTargetView, ID3D11DepthStencilView** ppDepthStencilView) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 1280;
    sd.BufferDesc.Height = 720;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = 0;

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, 1, D3D11_SDK_VERSION, &sd, ppSwapChain, ppDevice, &featureLevel, ppDeviceContext);

    if (FAILED(hr)) {
        return hr;
    }

    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = (*ppSwapChain)->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
    if (FAILED(hr)) {
        return hr;
    }

    hr = (*ppDevice)->CreateRenderTargetView(pBackBuffer, nullptr, ppRenderTargetView);
    pBackBuffer->Release();

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = 1280;
    depthDesc.Height = 720;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthDesc.CPUAccessFlags = 0;
    depthDesc.MiscFlags = 0;

    ID3D11Texture2D* pDepthStencilTexture = nullptr;

    hr = (*ppDevice)->CreateTexture2D(&depthDesc, nullptr, &pDepthStencilTexture);
    if (FAILED(hr)) {
        return hr;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc = {};
    depthViewDesc.Format = depthDesc.Format;
    depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthViewDesc.Texture2D.MipSlice = 0;

    hr = (*ppDevice)->CreateDepthStencilView(pDepthStencilTexture, &depthViewDesc, ppDepthStencilView);
    if (FAILED(hr)) {
        return hr;
    }

    return hr;
}

void HandleInput(double deltaTime, double& angle_y, double& angle_xz, double rotationSpeed) {
    // Поворот влево (стрелка влево)
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
        angle_y -= rotationSpeed * deltaTime;
    }
    // Поворот вправо (стрелка вправо)
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
        angle_y += rotationSpeed * deltaTime;
    }
    if (GetAsyncKeyState(VK_UP) & 0x8000) {
        angle_xz += rotationSpeed * deltaTime;
    }
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
        angle_xz -= rotationSpeed * deltaTime;
    }
    // проверка на выход из диапазона
    if (angle_y > 2 * DirectX::XM_PI) {
        angle_y -= 2 * DirectX::XM_PI;
    }
    if (angle_y < 0) {
        angle_y += 2 * DirectX::XM_PI;
    }
    if (angle_xz > DirectX::XM_PI / 2 - FLT_EPSILON)
    {
        angle_xz = DirectX::XM_PI / 2 - FLT_EPSILON;
    }
    if (angle_xz < -DirectX::XM_PI / 2 + FLT_EPSILON)
    {
        angle_xz = -DirectX::XM_PI / 2 + FLT_EPSILON;
    }
}

void UpdateRotation(double deltaTime, ID3D11DeviceContext* pDeviceContext, ID3D11Buffer* pGeomBuffer, double& angle_y, double& angle_xz) {
    static const double rotationViewSpeed = 1.0; // Скорость повота камеры
    HandleInput(deltaTime, angle_y, angle_xz, rotationViewSpeed);

    GeomBuffer geomBuffer;

    DirectX::CXMMATRIX offset = DirectX::XMMatrixTranslation(0.0f, 0.0f, 1.0f);
    DirectX::XMVECTOR rotationAxis = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f); // ось постоянного вращения куба
    rotationAxis = DirectX::XMVector3Normalize(rotationAxis);
    static float rotationAngle = 0.0f;
    static const double rotationModelSpeed = 0.01f;

    rotationAngle += rotationModelSpeed * deltaTime;
    if (rotationAngle > 2 * DirectX::XM_PI) {
        rotationAngle -= 2 * DirectX::XM_PI;
    }

    DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationAxis(rotationAxis, rotationAngle);

    geomBuffer.model = rotationMatrix;

    float cameraRadius = 1.0f;
    float cameraX = cameraRadius * sinf(static_cast<float>(angle_y)); // x = r * sin(angle)
    float cameraZ = cameraRadius * cosf(static_cast<float>(angle_y)); // z = r * cos(angle)
    float cameraY = cameraRadius * sinf(static_cast<float>(angle_xz));
    cameraX *= cosf(static_cast<float>(angle_xz));
    cameraZ *= cosf(static_cast<float>(angle_xz));

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(
        DirectX::XMVectorSet(cameraX, cameraY, cameraZ, 0.0f),
        DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
    );

    geomBuffer.view = DirectX::XMMatrixMultiply(view, offset);

    float fov = DirectX::XM_PI / 3.0f; // угол обзора 60 градусов
    float aspectRatio = 1280.0f / 720.0f;
    float nearZ = 0.1f; // ближняя плоскость отсечения
    float farZ = 100.0f; // дальняя плоскость отсечения
    geomBuffer.projection = DirectX::XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);

    pDeviceContext->UpdateSubresource(pGeomBuffer, 0, nullptr, &geomBuffer, 0, 0);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    HWND hWnd = CreateWindowInstance(hInstance, nCmdShow);
    if (!hWnd) {
        return -1;
    }

    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pDeviceContext = nullptr;
    IDXGISwapChain* pSwapChain = nullptr;
    ID3D11RenderTargetView* pRenderTargetView = nullptr;
    ID3D11Texture2D* pDepthStencilTexture = nullptr;
    ID3D11DepthStencilView* pDepthStencilView = nullptr;

    if (FAILED(InitDirectX(hWnd, &pDevice, &pDeviceContext, &pSwapChain, &pRenderTargetView, &pDepthStencilView))) {
        return -1;
    }

    ID3D11Buffer* pVertexBuffer = nullptr;
    ID3D11Buffer* pIndexBuffer = nullptr;
    ID3D11VertexShader* pVertexShader = nullptr;
    ID3D11PixelShader* pPixelShader = nullptr;
    ID3D11InputLayout* pInputLayout = nullptr;
    ID3D11Buffer* pGeomBuffer = nullptr;

    CreateVertexBuffer(pDevice, &pVertexBuffer);
    CreateIndexBuffer(pDevice, &pIndexBuffer);

    // Загрузка текстуры
    TextureDesc textureDesc;
    const std::wstring textureName = L"ds3.dds";
    if (!LoadDDS(textureName, textureDesc)) {
        return -1;
    }

    ID3D11Texture2D* pTexture = nullptr;
    HRESULT hr = CreateTexture(pDevice, textureDesc, &pTexture);
    if (FAILED(hr)) {
        return -1;
    }

    ID3D11ShaderResourceView* pTextureView = nullptr;
    hr = CreateShaderResourceView(pDevice, pTexture, textureDesc.fmt, &pTextureView);
    if (FAILED(hr)) {
        return -1;
    }

    // Создание семплера
    ID3D11SamplerState* pSampler = nullptr;
    hr = CreateSampler(pDevice, &pSampler);
    if (FAILED(hr)) {
        return -1;
    }

    ID3DBlob* pVertexShaderBlob = nullptr;
    ID3DBlob* pPixelShaderBlob = nullptr;
    CompileShader(vertexShaderCode, "vs", "vs_5_0", &pVertexShaderBlob);
    CompileShader(pixelShaderCode, "ps", "ps_5_0", &pPixelShaderBlob);

    pDevice->CreateVertexShader(pVertexShaderBlob->GetBufferPointer(), pVertexShaderBlob->GetBufferSize(), nullptr, &pVertexShader);
    pDevice->CreatePixelShader(pPixelShaderBlob->GetBufferPointer(), pPixelShaderBlob->GetBufferSize(), nullptr, &pPixelShader);

    CreateInputLayout(pDevice, &pInputLayout, pVertexShaderBlob);

    // Создание константного буфера
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(GeomBuffer);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    hr = pDevice->CreateBuffer(&desc, nullptr, &pGeomBuffer);
    if (FAILED(hr)) {
        return -1;
    }

    MSG msg = {};
    auto prevTime = std::chrono::high_resolution_clock::now();
    double angle_y = 0.0;
    double angle_xz = 0.0;
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = currentTime - prevTime;
            prevTime = currentTime;

            // Обновление вращения
            UpdateRotation(elapsed.count(), pDeviceContext, pGeomBuffer, angle_y, angle_xz);

            // Отрисовка
            Render(pDeviceContext, pRenderTargetView, pDepthStencilView, pIndexBuffer, pVertexBuffer, pInputLayout, pVertexShader, pPixelShader, pGeomBuffer, pSampler, pTextureView);
            pSwapChain->Present(1, 0);
        }
    }

    // Освобождение ресурсов
    if (pVertexBuffer) pVertexBuffer->Release();
    if (pIndexBuffer) pIndexBuffer->Release();
    if (pVertexShader) pVertexShader->Release();
    if (pPixelShader) pPixelShader->Release();
    if (pInputLayout) pInputLayout->Release();
    if (pRenderTargetView) pRenderTargetView->Release();
    if (pSwapChain) pSwapChain->Release();
    if (pDeviceContext) pDeviceContext->Release();
    if (pDevice) pDevice->Release();
    if (pGeomBuffer) pGeomBuffer->Release();
    if (pDepthStencilView) pDepthStencilView->Release();
    if (pDepthStencilTexture) pDepthStencilTexture->Release();
    if (pTextureView) pTextureView->Release();
    if (pSampler) pSampler->Release();
    if (pTexture) pTexture->Release();

    return (int)msg.wParam;
}