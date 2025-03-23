#include "lab6.h"

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

HRESULT CreateSphereVertexBuffer(ID3D11Device* pDevice, ID3D11Buffer** ppVertexBuffer) {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(SkyboxVertices);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = SkyboxVertices;

    return pDevice->CreateBuffer(&desc, &data, ppVertexBuffer);
}

HRESULT CreateSphereIndexBuffer(ID3D11Device* pDevice, ID3D11Buffer** ppIndexBuffer) {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(SkyboxIndices);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = SkyboxIndices;

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
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0}, };
    return pDevice->CreateInputLayout(inputDesc, ARRAYSIZE(inputDesc), pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), ppInputLayout);
}

bool LoadDDS(const std::wstring& filePath, TextureDesc& textureDesc) {
    HRESULT hr = DirectX::LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, textureDesc.image);
    if (FAILED(hr)) {
        return false;
    }

    const DirectX::TexMetadata& metadata = textureDesc.image.GetMetadata();
    textureDesc.width = static_cast<UINT32>(metadata.width);
    textureDesc.height = static_cast<UINT32>(metadata.height);
    textureDesc.fmt = metadata.format;
    textureDesc.mipmapsCount = static_cast<UINT32>(metadata.mipLevels);
    textureDesc.pData = textureDesc.image.GetPixels();

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

HRESULT CreateSphereTexture(ID3D11Device* pDevice, const TextureDesc* textureDesc, ID3D11Texture2D** ppCubemapTexture) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = textureDesc[0].width;
    desc.Height = textureDesc[0].height;
    desc.MipLevels = 1;
    desc.ArraySize = 6;
    desc.Format = textureDesc[0].fmt;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    UINT32 blockWidth = DivUp(desc.Width, 4u);
    UINT32 blockHeight = DivUp(desc.Height, 4u);
    UINT32 pitch = blockWidth * GetBytesPerBlock(desc.Format);

    D3D11_SUBRESOURCE_DATA data[6];
    for (UINT32 i = 0; i < 6; i++) {
        data[i].pSysMem = textureDesc[i].pData;
        data[i].SysMemPitch = pitch;
        data[i].SysMemSlicePitch = 0;
    }

    return pDevice->CreateTexture2D(&desc, data, ppCubemapTexture);
}

HRESULT CreateShaderResourceView(ID3D11Device* pDevice, ID3D11Texture2D* pTexture, DXGI_FORMAT textureFmt, ID3D11ShaderResourceView** ppTextureView) {
    D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format = textureFmt;
    desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipLevels = -1; // Используем все уровни мипмап
    desc.Texture2D.MostDetailedMip = 0;

    return pDevice->CreateShaderResourceView(pTexture, &desc, ppTextureView);
}

HRESULT CreateShaderSphereResourceView(ID3D11Device* pDevice, ID3D11Texture2D* pTexture, DXGI_FORMAT textureFmt, ID3D11ShaderResourceView** ppTextureView) {
    D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format = textureFmt;
    desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    desc.TextureCube.MipLevels = 1;
    desc.TextureCube.MostDetailedMip = 0;

    return pDevice->CreateShaderResourceView(pTexture, &desc, ppTextureView);
}

HRESULT CreateSampler(ID3D11Device* pDevice, ID3D11SamplerState** ppSampler) {
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = D3D11_FILTER_ANISOTROPIC;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = FLT_MAX;
    desc.MipLODBias = 0.0f;
    desc.MaxAnisotropy = 16;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;

    return pDevice->CreateSamplerState(&desc, ppSampler);
}

HRESULT CreateSquareVertexBuffer(ID3D11Device* pDevice, ID3D11Buffer** ppVertexBuffer) {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(SquareVertices);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = SquareVertices;

    return pDevice->CreateBuffer(&desc, &data, ppVertexBuffer);
}

HRESULT CreateSquareIndexBuffer(ID3D11Device* pDevice, ID3D11Buffer** ppIndexBuffer) {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(SquareIndices);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = SquareIndices;

    return pDevice->CreateBuffer(&desc, &data, ppIndexBuffer);
}

float CalculateDistance(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
    return sqrtf((a.x - b.x) * (a.x - b.x) +
        (a.y - b.y) * (a.y - b.y) +
        (a.z - b.z) * (a.z - b.z));
}

DirectX::XMFLOAT3 GetSquareCenter(UINT startVertex) {
    DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };

    // Суммируем координаты вершин квадрата
    for (UINT i = 0; i < 4; ++i) {
        center.x += SquareVertices[startVertex + i].x;
        center.y += SquareVertices[startVertex + i].y;
        center.z += SquareVertices[startVertex + i].z;
    }

    // Вычисляем среднее значение (центр)
    center.x /= 4.0f;
    center.y /= 4.0f;
    center.z /= 4.0f;

    return center;
}

void Render(ID3D11DeviceContext* pDeviceContext, ID3D11RenderTargetView* pRenderTargetView, ID3D11DepthStencilView* pDepthStencilView,
    ID3D11Buffer* pIndexBuffer, ID3D11Buffer* pVertexBuffer, ID3D11InputLayout* pInputLayout, ID3D11VertexShader* pVertexShader,
    ID3D11PixelShader* pPixelShader, ID3D11Buffer* pGeomBuffer, ID3D11Buffer* pGeomBuffer2, ID3D11SamplerState* pSampler, ID3D11ShaderResourceView* pTextureView,
    ID3D11Buffer* pSphereIndexBuffer, ID3D11Buffer* pSphereVertexBuffer, ID3D11InputLayout* pSphereInputLayout, ID3D11VertexShader* pSphereVertexShader,
    ID3D11PixelShader* pSpherePixelShader, ID3D11Buffer* pSphereGeomBuffer, ID3D11Buffer* pSphereSceneBuffer, ID3D11ShaderResourceView* pSphereTextureView,
    ID3D11Buffer* pSquareVertexBuffer, ID3D11Buffer* pSquareIndexBuffer, ID3D11InputLayout* pSquareInputLayout, ID3D11VertexShader* pSquareVertexShader,
    ID3D11PixelShader* pSquarePixelShader, ID3D11Buffer* pSquareGeomBuffer, ID3D11Buffer* pColorBuffer, ID3D11RasterizerState* pNoCullRasterizerState,
    ID3D11BlendState* pTransBlendState, ID3D11DepthStencilState* pNoWriteDepthStencilState, const DirectX::XMFLOAT3& cameraPosition, ID3D11Buffer* pSceneBuffer, ID3D11Buffer* pMaterialBuffer, ID3D11Buffer* pLightGeomBuffer, ID3D11PixelShader* pLightPixelShader, ID3D11ShaderResourceView* pTextureNormalView)
{
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
    pDeviceContext->PSSetConstantBuffers(1, 1, &pSceneBuffer);
    pDeviceContext->PSSetConstantBuffers(2, 1, &pMaterialBuffer);

    // cubemap
    pDeviceContext->IASetIndexBuffer(pSphereIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexSphereBuffers[] = { pSphereVertexBuffer };
    UINT sp_strides[] = { sizeof(SphereVertex) };
    UINT sp_offsets[] = { 0 };
    pDeviceContext->IASetVertexBuffers(0, 1, vertexSphereBuffers, sp_strides, sp_offsets);
    pDeviceContext->IASetInputLayout(pSphereInputLayout);
    pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pDeviceContext->VSSetShader(pSphereVertexShader, nullptr, 0);
    pDeviceContext->PSSetShader(pSpherePixelShader, nullptr, 0);
    pDeviceContext->VSSetConstantBuffers(1, 1, &pSphereGeomBuffer);
    pDeviceContext->VSSetConstantBuffers(0, 1, &pSphereSceneBuffer);
    pDeviceContext->PSSetSamplers(0, 1, &pSampler);
    pDeviceContext->PSSetShaderResources(0, 1, &pSphereTextureView);
    pDeviceContext->DrawIndexed(36, 0, 0);

    // кубы
    pDeviceContext->PSSetShaderResources(1, 1, &pTextureNormalView); // Карта нормалей (t1)
    pDeviceContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { pVertexBuffer };
    UINT strides[] = { sizeof(TextureTangentVertex) };
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

    pDeviceContext->VSSetConstantBuffers(0, 1, &pGeomBuffer2);
    pDeviceContext->DrawIndexed(36, 0, 0);

    // Отрисовка источника света
    pDeviceContext->VSSetConstantBuffers(0, 1, &pLightGeomBuffer);
    pDeviceContext->PSSetShader(pLightPixelShader, nullptr, 0);
    pDeviceContext->DrawIndexed(36, 0, 0);

    // Отрисовка квадратов
    pDeviceContext->RSSetState(pNoCullRasterizerState);
    pDeviceContext->OMSetBlendState(pTransBlendState, nullptr, 0xFFFFFFFF); // Устанавливаем состояние смешивания для прозрачности
    pDeviceContext->OMSetDepthStencilState(pNoWriteDepthStencilState, 0); // Отключаем запись в буфер глубины

    pDeviceContext->IASetIndexBuffer(pSquareIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexSquareBuffers[] = { pSquareVertexBuffer };
    UINT squareStrides[] = { sizeof(TextureNormalVertex) };
    UINT squareOffsets[] = { 0 };
    pDeviceContext->IASetVertexBuffers(0, 1, vertexSquareBuffers, squareStrides, squareOffsets);
    pDeviceContext->IASetInputLayout(pSquareInputLayout);
    pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pDeviceContext->VSSetShader(pSquareVertexShader, nullptr, 0);
    pDeviceContext->PSSetShader(pSquarePixelShader, nullptr, 0);
    pDeviceContext->VSSetConstantBuffers(0, 1, &pSquareGeomBuffer);

    // Информация о квадратах
    SquareInfo squares[] = {
        { GetSquareCenter(0), DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 0.8f), 0.0f, 0 }, // Красный квадрат (индексы 0-5)
        { GetSquareCenter(4), DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 0.8f), 0.0f, 6 }  // Желтый квадрат (индексы 6-11)
    };

    for (auto& square : squares) {
        square.distance = CalculateDistance(square.position, cameraPosition);
    }

    // Сортируем квадраты по расстоянию (от дальнего к ближнему)
    std::sort(squares, squares + 2, [](const SquareInfo& a, const SquareInfo& b) {
        return a.distance > b.distance;
        });
    // Отрисовка квадратов
    for (const auto& square : squares) {
        ColorBuffer colorBuffer;
        colorBuffer.color = square.color;
        pDeviceContext->UpdateSubresource(pColorBuffer, 0, nullptr, &colorBuffer, 0, 0);
        pDeviceContext->PSSetConstantBuffers(3, 1, &pColorBuffer);

        pDeviceContext->DrawIndexed(6, square.startIndex, 0);
    }

    pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    pDeviceContext->OMSetDepthStencilState(nullptr, 0); // Восстанавливаем состояние глубины
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
    ID3D11RenderTargetView** ppRenderTargetView, ID3D11DepthStencilView** ppDepthStencilView, ID3D11Texture2D** ppDepthBuffer, ID3D11DepthStencilState** ppDepthState) {
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

    //буффер глубины
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = 1280;
    depthDesc.Height = 720;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    //depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthDesc.CPUAccessFlags = 0;
    depthDesc.MiscFlags = 0;

    hr = (*ppDevice)->CreateTexture2D(&depthDesc, nullptr, ppDepthBuffer);
    if (FAILED(hr)) {
        return hr;
    }

    // Создание Depth Stencil View
    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc = {};
    depthViewDesc.Format = depthDesc.Format;
    depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthViewDesc.Texture2D.MipSlice = 0;

    hr = (*ppDevice)->CreateDepthStencilView(*ppDepthBuffer, &depthViewDesc, ppDepthStencilView);
    if (FAILED(hr)) {
        return hr;
    }

    D3D11_DEPTH_STENCIL_DESC deptStateDesc = {};
    deptStateDesc.DepthEnable = TRUE;
    deptStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    deptStateDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; // Используем LESS_EQUAL для корректной отрисовки
    deptStateDesc.StencilEnable = FALSE;

    return (*ppDevice)->CreateDepthStencilState(&deptStateDesc, ppDepthState);
}

void HandleInput(double deltaTime, double& angle_y, double& angle_xz, double rotationSpeed, double& cameraRadius) {
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

    // изменение расстояния от камеры
    if (GetAsyncKeyState('W') & 0x8000 && cameraRadius > 1.0) {
        cameraRadius -= rotationSpeed * 2.0 * deltaTime;
    }
    // Уменьшение расстояния (клавиша S)
    if (GetAsyncKeyState('S') & 0x8000 && cameraRadius < 100.0) {
        cameraRadius += rotationSpeed * 2.0 * deltaTime;
    }
}

void UpdateRotation(double deltaTime, ID3D11DeviceContext* pDeviceContext, ID3D11Buffer* pGeomBuffer, ID3D11Buffer* pGeomBuffer2, ID3D11Buffer* pLightGeomBuffer, ID3D11Buffer* pSphereGeomBuffer,
    ID3D11Buffer* pSphereSceneBuffer, ID3D11Buffer* pSquareGeomBuffer, double& angle_y, double& angle_xz, double& cameraRadius, DirectX::XMFLOAT3& cameraPosition, ID3D11Buffer* pSceneBuffer) {
    static const double rotationViewSpeed = 1.0; // Скорость повота камеры
    HandleInput(deltaTime, angle_y, angle_xz, rotationViewSpeed, cameraRadius);

    GeomBuffer geomBuffer;
    GeomBuffer geomBuffer2;
    GeomBuffer geomLightBuffer;
    GeomBuffer sphereGeomBuffer;
    SceneBuffer sphereSceneBuffer;

    // зададим источник освещения
    SceneBuffer sceneBuffer = {};
    sceneBuffer.lightCount = DirectX::XMFLOAT4(1, 0, 0, 0); // Один источник света
    sceneBuffer.lights[0].pos = DirectX::XMFLOAT4(0.5f, 0.7f, -0.5f, 1.0f);
    sceneBuffer.lights[0].color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f); // Белый цвет
    sceneBuffer.ambientColor = DirectX::XMFLOAT4(0.05f, 0.05f, 0.05f, 1.0f); // Окружающее освещение
    sceneBuffer.cameraPos = DirectX::XMFLOAT4(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f); // Позиция камеры
    pDeviceContext->UpdateSubresource(pSceneBuffer, 0, nullptr, &sceneBuffer, 0, 0);

    DirectX::CXMMATRIX offset = DirectX::XMMatrixTranslation(0.0f, 0.0f, 1.0f);
    DirectX::XMVECTOR rotationAxis = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f); // ось постоянного вращения куба
    rotationAxis = DirectX::XMVector3Normalize(rotationAxis);
    static float rotationAngle = 0.0f;
    static const double rotationModelSpeed = 0.05f;

    rotationAngle += rotationModelSpeed * deltaTime;
    if (rotationAngle > 2 * DirectX::XM_PI) {
        rotationAngle -= 2 * DirectX::XM_PI;
    }

    DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationAxis(rotationAxis, rotationAngle);

    geomBuffer.model = rotationMatrix;
    geomBuffer2.model = DirectX::XMMatrixMultiply(DirectX::XMMatrixIdentity(), DirectX::XMMatrixTranslation(2.0f, 0.0f, 0.0f));
    sphereGeomBuffer.model = DirectX::XMMatrixIdentity();
    geomLightBuffer.model = DirectX::XMMatrixScaling(0.1f, 0.1f, 0.1f) *
        DirectX::XMMatrixTranslation(sceneBuffer.lights[0].pos.x, sceneBuffer.lights[0].pos.y, sceneBuffer.lights[0].pos.z);

    DirectX::XMMATRIX normalMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, geomBuffer.model));
    geomBuffer.normalMatrix = normalMatrix;
    DirectX::XMMATRIX normalMatrix2 = XMMatrixTranspose(XMMatrixInverse(nullptr, geomBuffer2.model));
    geomBuffer2.normalMatrix = normalMatrix2;

    float cameraX = cameraRadius * sinf(static_cast<float>(angle_y)); // x = r * sin(angle)
    float cameraZ = cameraRadius * cosf(static_cast<float>(angle_y)); // z = r * cos(angle)
    float cameraY = cameraRadius * sinf(static_cast<float>(angle_xz));
    cameraX *= cosf(static_cast<float>(angle_xz));
    cameraZ *= cosf(static_cast<float>(angle_xz));

    cameraPosition = DirectX::XMFLOAT3(cameraX, cameraY, cameraZ);

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(
        DirectX::XMVectorSet(cameraX, cameraY, cameraZ, 0.0f),
        DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
    );

    geomBuffer.view = DirectX::XMMatrixMultiply(view, offset);
    geomBuffer2.view = DirectX::XMMatrixMultiply(view, offset);
    geomLightBuffer.view = geomBuffer.view;

    float fov = DirectX::XM_PI / 3.0f; // угол обзора 60 градусов
    float aspectRatio = 1280.0f / 720.0f;
    float nearZ = 0.1f; // ближняя плоскость отсечения
    float farZ = 1000.0f; // дальняя плоскость отсечения
    auto proj = DirectX::XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);

    geomBuffer.projection = proj;
    geomBuffer2.projection = proj;
    geomLightBuffer.projection = proj;
    //sphereGeomBuffer.projection = DirectX::XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);
    sphereSceneBuffer.vp = geomBuffer.view * geomBuffer.projection;
    sphereSceneBuffer.cameraPos.x = cameraX;
    sphereSceneBuffer.cameraPos.y = cameraY;
    sphereSceneBuffer.cameraPos.z = cameraZ;

    pDeviceContext->UpdateSubresource(pGeomBuffer, 0, nullptr, &geomBuffer, 0, 0);
    pDeviceContext->UpdateSubresource(pGeomBuffer2, 0, nullptr, &geomBuffer2, 0, 0);
    pDeviceContext->UpdateSubresource(pLightGeomBuffer, 0, nullptr, &geomLightBuffer, 0, 0);
    pDeviceContext->UpdateSubresource(pSphereGeomBuffer, 0, nullptr, &sphereGeomBuffer, 0, 0);
    pDeviceContext->UpdateSubresource(pSphereSceneBuffer, 0, nullptr, &sphereSceneBuffer, 0, 0);

    // Обновление преобразований для квадратов
    GeomBuffer squareGeomBuffer;
    squareGeomBuffer.model = DirectX::XMMatrixIdentity();
    squareGeomBuffer.view = geomBuffer.view;
    squareGeomBuffer.projection = geomBuffer.projection;
    DirectX::XMMATRIX normalMatrixSquare = XMMatrixTranspose(XMMatrixInverse(nullptr, squareGeomBuffer.model));
    squareGeomBuffer.normalMatrix = normalMatrixSquare;

    pDeviceContext->UpdateSubresource(pSquareGeomBuffer, 0, nullptr, &squareGeomBuffer, 0, 0);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
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
    ID3D11Texture2D* pDepthBuffer = nullptr;
    ID3D11DepthStencilState* pDepthState = nullptr;

    if (FAILED(InitDirectX(hWnd, &pDevice, &pDeviceContext, &pSwapChain, &pRenderTargetView, &pDepthStencilView, &pDepthBuffer, &pDepthState))) {
        return -1;
    }

    // Создание константного буфера для освещения
    ID3D11Buffer* pSceneBuffer = nullptr;

    D3D11_BUFFER_DESC sceneBufferDesc = {};
    sceneBufferDesc.ByteWidth = sizeof(SceneBuffer);
    sceneBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    sceneBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    sceneBufferDesc.CPUAccessFlags = 0;
    sceneBufferDesc.MiscFlags = 0;
    sceneBufferDesc.StructureByteStride = 0;

    HRESULT hr = pDevice->CreateBuffer(&sceneBufferDesc, nullptr, &pSceneBuffer);
    if (FAILED(hr)) {
        return -1;
    }
    ID3D11Buffer* pMaterialBuffer = nullptr;

    D3D11_BUFFER_DESC materialBufferDesc = {};
    materialBufferDesc.ByteWidth = sizeof(MaterialBuffer);
    materialBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    materialBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    materialBufferDesc.CPUAccessFlags = 0;
    materialBufferDesc.MiscFlags = 0;
    materialBufferDesc.StructureByteStride = 0;

    MaterialBuffer materialBuffer = {};
    materialBuffer.shine = DirectX::XMFLOAT4(32.0f, 0.0f, 0.0f, 0.0f); // Коэффициент блеска

    D3D11_SUBRESOURCE_DATA materialData = {};
    materialData.pSysMem = &materialBuffer; // Указываем данные для инициализации буфера

    hr = pDevice->CreateBuffer(&materialBufferDesc, &materialData, &pMaterialBuffer);
    if (FAILED(hr)) {
        return -1;
    }

    // Создание ресурсов для квадратов
    ID3D11Buffer* pSquareVertexBuffer = nullptr;
    ID3D11Buffer* pSquareIndexBuffer = nullptr;
    ID3D11VertexShader* pSquareVertexShader = nullptr;
    ID3D11PixelShader* pSquarePixelShader = nullptr;
    ID3D11InputLayout* pSquareInputLayout = nullptr;
    ID3D11Buffer* pSquareGeomBuffer = nullptr;

    CreateSquareVertexBuffer(pDevice, &pSquareVertexBuffer);
    CreateSquareIndexBuffer(pDevice, &pSquareIndexBuffer);

    ID3DBlob* pSquareVertexShaderBlob = nullptr;
    ID3DBlob* pSquarePixelShaderBlob = nullptr;
    CompileShader(vertexColorShaderCode, "vs", "vs_5_0", &pSquareVertexShaderBlob);
    CompileShader(pixelColorShaderCode, "ps", "ps_5_0", &pSquarePixelShaderBlob);

    pDevice->CreateVertexShader(pSquareVertexShaderBlob->GetBufferPointer(), pSquareVertexShaderBlob->GetBufferSize(), nullptr, &pSquareVertexShader);
    pDevice->CreatePixelShader(pSquarePixelShaderBlob->GetBufferPointer(), pSquarePixelShaderBlob->GetBufferSize(), nullptr, &pSquarePixelShader);
    CreateInputLayout(pDevice, &pSquareInputLayout, pSquareVertexShaderBlob);

    D3D11_BUFFER_DESC squareGeomDesc = {};
    squareGeomDesc.ByteWidth = sizeof(GeomBuffer);
    squareGeomDesc.Usage = D3D11_USAGE_DEFAULT;
    squareGeomDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    squareGeomDesc.CPUAccessFlags = 0;
    squareGeomDesc.MiscFlags = 0;
    squareGeomDesc.StructureByteStride = 0;

    hr = pDevice->CreateBuffer(&squareGeomDesc, nullptr, &pSquareGeomBuffer);
    if (FAILED(hr)) {
        return -1;
    }

    ID3D11Buffer* pColorBuffer = nullptr;

    D3D11_BUFFER_DESC colorBufferDesc = {};
    colorBufferDesc.ByteWidth = sizeof(ColorBuffer);
    colorBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    colorBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    colorBufferDesc.CPUAccessFlags = 0;
    colorBufferDesc.MiscFlags = 0;
    colorBufferDesc.StructureByteStride = 0;

    hr = pDevice->CreateBuffer(&colorBufferDesc, nullptr, &pColorBuffer);
    if (FAILED(hr)) {
        return -1;
    }

    ID3D11RasterizerState* pNoCullRasterizerState = nullptr;

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID; // Режим заполнения (сплошной)
    rasterDesc.CullMode = D3D11_CULL_NONE;  // Отключаем отсечение задних граней
    rasterDesc.FrontCounterClockwise = FALSE; // Указываем порядок вершин (по часовой стрелке)
    rasterDesc.DepthBias = 0;
    rasterDesc.DepthBiasClamp = 0.0f;
    rasterDesc.SlopeScaledDepthBias = 0.0f;
    rasterDesc.DepthClipEnable = TRUE;
    rasterDesc.ScissorEnable = FALSE;
    rasterDesc.MultisampleEnable = FALSE;
    rasterDesc.AntialiasedLineEnable = FALSE;

    hr = pDevice->CreateRasterizerState(&rasterDesc, &pNoCullRasterizerState);
    if (FAILED(hr)) {
        return -1;
    }

    ID3D11BlendState* pTransBlendState = nullptr;

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;

    hr = pDevice->CreateBlendState(&blendDesc, &pTransBlendState);
    if (FAILED(hr)) {
        return -1;
    }

    ID3D11DepthStencilState* pNoWriteDepthStencilState = nullptr;

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = TRUE; // Тестирование глубины включено
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // Отключаем запись в буфер глубины
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS; // Стандартное сравнение глубины
    depthStencilDesc.StencilEnable = FALSE; // Отключаем тестирование трафарета

    hr = pDevice->CreateDepthStencilState(&depthStencilDesc, &pNoWriteDepthStencilState);
    if (FAILED(hr)) {
        return -1;
    }

    // небесная сфера
    ID3D11Buffer* pSphereVertexBuffer = nullptr;
    ID3D11Buffer* pSphereIndexBuffer = nullptr;
    ID3D11VertexShader* pSphereVertexShader = nullptr;
    ID3D11PixelShader* pSpherePixelShader = nullptr;
    ID3D11InputLayout* pSphereInputLayout = nullptr;
    ID3D11Buffer* pSphereGeomBuffer = nullptr;
    ID3D11Buffer* pSphereSceneBuffer = nullptr;

    CreateSphereVertexBuffer(pDevice, &pSphereVertexBuffer);
    CreateSphereIndexBuffer(pDevice, &pSphereIndexBuffer);

    // Загрузка текстуры сферы
    const std::wstring TextureNames[6] = { L"space.dds", L"space.dds", L"space.dds", L"space.dds", L"space.dds", L"space.dds" };
    TextureDesc texDescs[6];
    for (int i = 0; i < 6; i++)
    {
        if (!LoadDDS(TextureNames[i].c_str(), texDescs[i]))
        {
            return -1;
        }
    }
    ID3D11Texture2D* pSphereTexture = nullptr;

    hr = CreateSphereTexture(pDevice, texDescs, &pSphereTexture);
    if (FAILED(hr)) {
        return -1;
    }

    ID3D11ShaderResourceView* pSphereTextureView = nullptr;
    hr = CreateShaderSphereResourceView(pDevice, pSphereTexture, texDescs[0].fmt, &pSphereTextureView);
    if (FAILED(hr)) {
        return -1;
    }

    ID3DBlob* pSphereVertexShaderBlob = nullptr;
    ID3DBlob* pSpherePixelShaderBlob = nullptr;
    CompileShader(vertexSphereShaderCode, "vs", "vs_5_0", &pSphereVertexShaderBlob);
    CompileShader(pixelSphereShaderCode, "ps", "ps_5_0", &pSpherePixelShaderBlob);

    pDevice->CreateVertexShader(pSphereVertexShaderBlob->GetBufferPointer(), pSphereVertexShaderBlob->GetBufferSize(), nullptr, &pSphereVertexShader);
    pDevice->CreatePixelShader(pSpherePixelShaderBlob->GetBufferPointer(), pSpherePixelShaderBlob->GetBufferSize(), nullptr, &pSpherePixelShader);
    CreateInputLayout(pDevice, &pSphereInputLayout, pSphereVertexShaderBlob); // надо ли???

    // Создание константного буфера
    D3D11_BUFFER_DESC Spheredesc = {};
    Spheredesc.ByteWidth = sizeof(GeomBuffer);
    Spheredesc.Usage = D3D11_USAGE_DEFAULT;
    Spheredesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Spheredesc.CPUAccessFlags = 0;
    Spheredesc.MiscFlags = 0;
    Spheredesc.StructureByteStride = 0;

    hr = pDevice->CreateBuffer(&Spheredesc, nullptr, &pSphereGeomBuffer);
    if (FAILED(hr)) {
        return -1;
    }

    // Создание константного буфера
    D3D11_BUFFER_DESC Scenedesc = {};
    Scenedesc.ByteWidth = sizeof(SceneBuffer);
    Scenedesc.Usage = D3D11_USAGE_DEFAULT;
    Scenedesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Scenedesc.CPUAccessFlags = 0;
    Scenedesc.MiscFlags = 0;
    Scenedesc.StructureByteStride = 0;

    hr = pDevice->CreateBuffer(&Scenedesc, nullptr, &pSphereSceneBuffer);
    if (FAILED(hr)) {
        return -1;
    }

    // куб
    ID3D11Buffer* pVertexBuffer = nullptr;
    ID3D11Buffer* pIndexBuffer = nullptr;
    ID3D11VertexShader* pVertexShader = nullptr;
    ID3D11PixelShader* pPixelShader = nullptr;
    ID3D11InputLayout* pInputLayout = nullptr;
    ID3D11Buffer* pGeomBuffer = nullptr;
    ID3D11Buffer* pGeomBuffer2 = nullptr;
    ID3D11Buffer* pLightGeomBuffer = nullptr;
    ID3D11PixelShader* pLightPixelShader = nullptr;

    CreateVertexBuffer(pDevice, &pVertexBuffer);
    CreateIndexBuffer(pDevice, &pIndexBuffer);

    // Загрузка текстуры
    TextureDesc textureDesc;
    const std::wstring textureName = L"texture.dds";
    if (!LoadDDS(textureName, textureDesc)) {
        return -1;
    }

    ID3D11Texture2D* pTexture = nullptr;
    hr = CreateTexture(pDevice, textureDesc, &pTexture);
    if (FAILED(hr)) {
        return -1;
    }

    ID3D11ShaderResourceView* pTextureView = nullptr;
    hr = CreateShaderResourceView(pDevice, pTexture, textureDesc.fmt, &pTextureView);
    if (FAILED(hr)) {
        return -1;
    }

    // Загрузка текстуры-карты нормалей
    TextureDesc textureNormDesc;
    const std::wstring textureNormalName = L"normal_map.dds";
    if (!LoadDDS(textureNormalName, textureNormDesc)) {
        return -1;
    }

    ID3D11Texture2D* pNormalTexture = nullptr;
    hr = CreateTexture(pDevice, textureNormDesc, &pNormalTexture);
    if (FAILED(hr)) {
        return -1;
    }

    ID3D11ShaderResourceView* pTextureNormalView = nullptr;
    hr = CreateShaderResourceView(pDevice, pNormalTexture, textureNormDesc.fmt, &pTextureNormalView);
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
    ID3DBlob* pLightPixelShaderBlob = nullptr;
    CompileShader(vertexShaderCode, "vs", "vs_5_0", &pVertexShaderBlob);
    CompileShader(pixelShaderCode, "ps", "ps_5_0", &pPixelShaderBlob);
    CompileShader(pixelLightShaderCode, "ps", "ps_5_0", &pLightPixelShaderBlob);


    pDevice->CreateVertexShader(pVertexShaderBlob->GetBufferPointer(), pVertexShaderBlob->GetBufferSize(), nullptr, &pVertexShader);
    pDevice->CreatePixelShader(pPixelShaderBlob->GetBufferPointer(), pPixelShaderBlob->GetBufferSize(), nullptr, &pPixelShader);
    pDevice->CreatePixelShader(pLightPixelShaderBlob->GetBufferPointer(), pLightPixelShaderBlob->GetBufferSize(), nullptr, &pLightPixelShader);
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

    D3D11_BUFFER_DESC desc2 = {};
    desc2.ByteWidth = sizeof(GeomBuffer);
    desc2.Usage = D3D11_USAGE_DEFAULT;
    desc2.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc2.CPUAccessFlags = 0;
    desc2.MiscFlags = 0;
    desc2.StructureByteStride = 0;

    hr = pDevice->CreateBuffer(&desc2, nullptr, &pGeomBuffer2);
    if (FAILED(hr)) {
        return -1;
    }

    D3D11_BUFFER_DESC desc3 = {};
    desc3.ByteWidth = sizeof(GeomBuffer);
    desc3.Usage = D3D11_USAGE_DEFAULT;
    desc3.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc3.CPUAccessFlags = 0;
    desc3.MiscFlags = 0;
    desc3.StructureByteStride = 0;

    hr = pDevice->CreateBuffer(&desc3, nullptr, &pLightGeomBuffer);
    if (FAILED(hr)) {
        return -1;
    }

    MSG msg = {};
    auto prevTime = std::chrono::high_resolution_clock::now();
    double angle_y = 0.0;
    double angle_xz = 0.5;
    double cameraRadius = 2.0;
    DirectX::XMFLOAT3 cameraPosition = { 0.0f, 0.0f, 0.0f };
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
            UpdateRotation(elapsed.count(), pDeviceContext, pGeomBuffer, pGeomBuffer2, pLightGeomBuffer, pSphereGeomBuffer, pSphereSceneBuffer, pSquareGeomBuffer, angle_y, angle_xz, cameraRadius, cameraPosition, pSceneBuffer);

            // Отрисовка
            Render(pDeviceContext, pRenderTargetView, pDepthStencilView, pIndexBuffer, pVertexBuffer, pInputLayout, pVertexShader, pPixelShader, pGeomBuffer, pGeomBuffer2, pSampler, pTextureView,
                pSphereIndexBuffer, pSphereVertexBuffer, pSphereInputLayout, pSphereVertexShader, pSpherePixelShader, pSphereGeomBuffer, pSphereSceneBuffer, pSphereTextureView,
                pSquareVertexBuffer, pSquareIndexBuffer, pSquareInputLayout, pSquareVertexShader, pSquarePixelShader, pSquareGeomBuffer, pColorBuffer, pNoCullRasterizerState, pTransBlendState, pNoWriteDepthStencilState, cameraPosition, pSceneBuffer, pMaterialBuffer, pLightGeomBuffer, pLightPixelShader, pTextureNormalView);
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

    if (pSphereVertexBuffer) pSphereVertexBuffer->Release();
    if (pSphereIndexBuffer) pSphereIndexBuffer->Release();
    if (pSphereVertexShader) pSphereVertexShader->Release();
    if (pSpherePixelShader) pSpherePixelShader->Release();
    if (pSphereInputLayout) pSphereInputLayout->Release();
    if (pSphereGeomBuffer) pSphereGeomBuffer->Release();
    if (pSphereTextureView) pSphereTextureView->Release();
    if (pSphereTexture) pSphereTexture->Release();

    if (pSquareVertexBuffer) pSquareVertexBuffer->Release();
    if (pSquareIndexBuffer) pSquareIndexBuffer->Release();
    if (pSquareInputLayout) pSquareInputLayout->Release();
    if (pSquareVertexShader) pSquareVertexShader->Release();
    if (pSquarePixelShader) pSquarePixelShader->Release();
    if (pSquareGeomBuffer) pSquareGeomBuffer->Release();
    if (pColorBuffer) pColorBuffer->Release();
    if (pNoCullRasterizerState) pNoCullRasterizerState->Release();
    if (pTransBlendState) pTransBlendState->Release();
    if (pNoWriteDepthStencilState) pNoWriteDepthStencilState->Release();

    return (int)msg.wParam;
}