#include <d3d11.h>
#include "lab2.h"
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cmath>
#include <string>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct Vertex {
    float x, y, z;
    COLORREF color;
};

static const Vertex Vertices[] = {
    {-0.5f, -0.5f, 0.0f, RGB(255, 0, 0)},
    { 0.5f, -0.5f, 0.0f, RGB(0, 255, 0)},
    { 0.0f,  0.5f, 0.0f, RGB(0, 0, 255)}
};

static const USHORT Indices[] = { 0, 2, 1 };

const char* vertexShaderCode = R"(
struct VSInput {
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct VSOutput {
    float4 pos : SV_Position;
    float4 color : COLOR;
};

VSOutput vs(VSInput vertex) {
    VSOutput result;
    result.pos = float4(vertex.pos, 1.0);
    result.color = vertex.color;
    return result;
}
)";

const char* pixelShaderCode = R"(
struct VSOutput {
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

void Render(ID3D11DeviceContext* pDeviceContext, ID3D11RenderTargetView* pRenderTargetView, ID3D11Buffer* pIndexBuffer, ID3D11Buffer* pVertexBuffer, ID3D11InputLayout* pInputLayout, ID3D11VertexShader* pVertexShader, ID3D11PixelShader* pPixelShader) {
    static const FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Черный цвет
    pDeviceContext->ClearRenderTargetView(pRenderTargetView, clearColor);
    pDeviceContext->OMSetRenderTargets(1, &pRenderTargetView, nullptr);

    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = 800;
    viewport.Height = 600;
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
    pDeviceContext->DrawIndexed(3, 0, 0);
}

HWND CreateWindowInstance(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = DefWindowProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"Lab2WindowClass";
    RegisterClassEx(&wcex);

    HWND hWnd = CreateWindow(wcex.lpszClassName, L"Lab2", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) {
        return nullptr;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return hWnd;
}

HRESULT InitDirectX(HWND hWnd, ID3D11Device** ppDevice, ID3D11DeviceContext** ppDeviceContext, IDXGISwapChain** ppSwapChain, ID3D11RenderTargetView** ppRenderTargetView) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

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

    return hr;
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

    if (FAILED(InitDirectX(hWnd, &pDevice, &pDeviceContext, &pSwapChain, &pRenderTargetView))) {
        return -1;
    }

    ID3D11Buffer* pVertexBuffer = nullptr;
    ID3D11Buffer* pIndexBuffer = nullptr;
    ID3D11VertexShader* pVertexShader = nullptr;
    ID3D11PixelShader* pPixelShader = nullptr;
    ID3D11InputLayout* pInputLayout = nullptr;

    CreateVertexBuffer(pDevice, &pVertexBuffer);
    CreateIndexBuffer(pDevice, &pIndexBuffer);

    ID3DBlob* pVertexShaderBlob = nullptr;
    ID3DBlob* pPixelShaderBlob = nullptr;
    CompileShader(vertexShaderCode, "vs", "vs_5_0", &pVertexShaderBlob);
    CompileShader(pixelShaderCode, "ps", "ps_5_0", &pPixelShaderBlob);

    pDevice->CreateVertexShader(pVertexShaderBlob->GetBufferPointer(), pVertexShaderBlob->GetBufferSize(), nullptr, &pVertexShader);
    pDevice->CreatePixelShader(pPixelShaderBlob->GetBufferPointer(), pPixelShaderBlob->GetBufferSize(), nullptr, &pPixelShader);

    CreateInputLayout(pDevice, &pInputLayout, pVertexShaderBlob);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            Render(pDeviceContext, pRenderTargetView, pIndexBuffer, pVertexBuffer, pInputLayout, pVertexShader, pPixelShader);
            pSwapChain->Present(0, 0);
        }
    }

    if (pVertexBuffer) pVertexBuffer->Release();
    if (pIndexBuffer) pIndexBuffer->Release();
    if (pVertexShader) pVertexShader->Release();
    if (pPixelShader) pPixelShader->Release();
    if (pInputLayout) pInputLayout->Release();
    if (pRenderTargetView) pRenderTargetView->Release();
    if (pSwapChain) pSwapChain->Release();
    if (pDeviceContext) pDeviceContext->Release();
    if (pDevice) pDevice->Release();

    return (int)msg.wParam;
}





