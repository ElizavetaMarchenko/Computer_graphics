#include <d3d11.h>
#include "lab3.h"
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cmath>
#include <string>
#include <vector>
#include <chrono>
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct Vertex {
    float x, y, z;
    COLORREF color;
};

static const Vertex Vertices[] = {
    // Передняя грань
    {-0.5f, -0.5f,  0.5f, RGB(255, 0, 0)},   // 0: Красный
    { 0.5f, -0.5f,  0.5f, RGB(0, 255, 0)},   // 1: Зеленый
    { 0.5f,  0.5f,  0.5f, RGB(0, 0, 255)},   // 2: Синий
    {-0.5f,  0.5f,  0.5f, RGB(255, 255, 0)}, // 3: Желтый

    // Задняя грань
    {-0.5f, -0.5f, -0.5f, RGB(255, 0, 255)}, // 4: Пурпурный
    { 0.5f, -0.5f, -0.5f, RGB(0, 255, 255)}, // 5: Голубой
    { 0.5f,  0.5f, -0.5f, RGB(255, 165, 0)}, // 6: Оранжевый
    {-0.5f,  0.5f, -0.5f, RGB(128, 0, 128)}  // 7: Фиолетовый
};

struct GeomBuffer {
    DirectX::XMMATRIX model;
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX projection;
};

static const USHORT Indices[] = {
    // Передняя грань
    0, 1, 2,
    0, 2, 3,
    // Задняя грань
    4, 6, 5,
    4, 7, 6,
    // Верхняя грань
    3, 2, 6,
    3, 6, 7,
    // Нижняя грань
    0, 5, 1,
    0, 4, 5,
    // Левая грань
    0, 3, 7,
    0, 7, 4,
    // Правая грань
    1, 5, 6,
    1, 6, 2
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
    float4 color : COLOR;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float4 color : COLOR;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;
    float4 pos = float4(vertex.pos, 1.0);
    pos = mul(model, pos);
    pos = mul(view, pos);
    pos = mul(projection, pos);
    result.pos = pos;
    result.color = vertex.color;
    return result;
}
)";

const char* pixelShaderCode = R"(
struct VSOutput
{
    float4 pos : SV_Position;
    float4 color : COLOR;
};

float4 ps(VSOutput pixel) : SV_Target{
    return pixel.color;
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
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    return pDevice->CreateInputLayout(inputDesc, ARRAYSIZE(inputDesc), pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), ppInputLayout);
}

void Render(ID3D11DeviceContext* pDeviceContext, ID3D11RenderTargetView* pRenderTargetView, ID3D11DepthStencilView* pDepthStencilView, ID3D11Buffer* pIndexBuffer, ID3D11Buffer* pVertexBuffer, ID3D11InputLayout* pInputLayout, ID3D11VertexShader* pVertexShader, ID3D11PixelShader* pPixelShader, ID3D11Buffer* pGeomBuffer) {
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
    UINT strides[] = { sizeof(Vertex) };
    UINT offsets[] = { 0 };
    pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    pDeviceContext->IASetInputLayout(pInputLayout);
    pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pDeviceContext->VSSetShader(pVertexShader, nullptr, 0);
    pDeviceContext->PSSetShader(pPixelShader, nullptr, 0);
    pDeviceContext->VSSetConstantBuffers(0, 1, &pGeomBuffer);
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
    wcex.lpszClassName = L"Lab2WindowClass";
    RegisterClassEx(&wcex);

    HWND hWnd = CreateWindow(wcex.lpszClassName, L"Lab2", WS_OVERLAPPEDWINDOW,
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
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // Эффект замены буферов
    sd.Flags = 0; // Дополнительные флаги

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
    if (angle_xz < -DirectX::XM_PI / 2 - FLT_EPSILON)
    {
        angle_xz = -DirectX::XM_PI / 2 - FLT_EPSILON;
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
    static const double rotationModelSpeed = 0.03f;

    rotationAngle += rotationModelSpeed * deltaTime;
    if (rotationAngle > 2 * DirectX::XM_PI) {
        rotationAngle -= 2 * DirectX::XM_PI;
    }

    DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationAxis(rotationAxis, rotationAngle);

    geomBuffer.model = rotationMatrix;

    float cameraRadius = 0.5f;
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

    HRESULT hr = pDevice->CreateBuffer(&desc, nullptr, &pGeomBuffer);
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
            Render(pDeviceContext, pRenderTargetView, pDepthStencilView, pIndexBuffer, pVertexBuffer, pInputLayout, pVertexShader, pPixelShader, pGeomBuffer);
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

    return (int)msg.wParam;
}

