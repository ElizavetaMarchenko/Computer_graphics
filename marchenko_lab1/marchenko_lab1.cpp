#include "framework.h"
#include "marchenko_lab1.h"

#include <d3d11.h>
#include <dxgi.h>
#include <cmath>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Глобальные переменные для DirectX
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swap_chain = nullptr;
ID3D11RenderTargetView* g_render_target_view = nullptr;

// Функция для обработки сообщения WM_SIZE
void OnResize(UINT width, UINT height)
{
    if (g_device && g_swap_chain && g_render_target_view)
    {
        g_render_target_view->Release();

        DXGI_SWAP_CHAIN_DESC desc;
        g_swap_chain->GetDesc(&desc);
        g_swap_chain->ResizeBuffers(desc.BufferCount, width, height, desc.BufferDesc.Format, desc.Flags);

        ID3D11Texture2D* back_buffer;
        g_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer));
        g_device->CreateRenderTargetView(back_buffer, NULL, &g_render_target_view);
        back_buffer->Release();

        D3D11_VIEWPORT vp;
        vp.Width = static_cast<float>(width);
        vp.Height = static_cast<float>(height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        g_context->RSSetViewports(1, &vp);
    }
}

void Render()
{
    DWORD current_time = GetTickCount();
    float time_factor = ((float)(current_time % 10000)) / 10000.0f; // каждые 10 секунд цвет меняется с красного на зеленый и обратно

    float red = abs(sin(time_factor * 3.1415926));
    float green = 1.0f - red;

    const float clear_color[] = { red, green, 0.0f, 1.0f };
    g_context->ClearRenderTargetView(g_render_target_view, clear_color);
    g_swap_chain->Present(1, 0);
}

// Обработчик сообщений Windows
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
            OnResize(LOWORD(lParam), HIWORD(lParam)); // Обновляем размеры текстуры при изменении окна
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DxApp";
    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(
        0,
        L"DxApp",
        L"DirectX Application",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    ShowWindow(hWnd, nCmdShow);

    // Инициализация DirectX
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D_FEATURE_LEVEL feature_level;
    D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &sd,
        &g_swap_chain,
        &g_device,
        &feature_level,
        &g_context
    );

    ID3D11Texture2D* back_buffer;
    g_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer));
    g_device->CreateRenderTargetView(back_buffer, nullptr, &g_render_target_view);
    back_buffer->Release();

    D3D11_VIEWPORT vp;
    vp.Width = 800.0f;
    vp.Height = 600.0f;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    g_context->RSSetViewports(1, &vp);

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Render();
        }
    }

    if (g_render_target_view) g_render_target_view->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
    if (g_swap_chain) g_swap_chain->Release();

    UnregisterClass(L"DxApp", hInstance);

    return 0;
}
