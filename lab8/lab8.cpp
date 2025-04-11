#include "lab8.h"

ID3D11Texture2D* m_pColorBuffer = nullptr;
ID3D11RenderTargetView* m_pColorBufferRTV = nullptr;
ID3D11ShaderResourceView* m_pColorBufferSRV = nullptr;
ID3D11VertexShader* m_pPostProcessVS = nullptr;
ID3D11PixelShader* m_pPostProcessPS = nullptr;
UINT m_width = 1280;
UINT m_height = 720;

// Запросы к GPU
ID3D11Query* m_pPipelineStatsQuery[10] = { nullptr };
UINT m_curQueryFrame = 0;
UINT m_lastCompletedQueryFrame = 0;
int m_gpuVisibleInstances = 0;
int m_cpuVisibleInstances = 0;
const UINT QUERY_FRAME_DELAY = 3; // Задержка в 3 кадра

HRESULT InitQueries(ID3D11Device* pDevice)
{
    D3D11_QUERY_DESC queryDesc = {};
    queryDesc.Query = D3D11_QUERY_PIPELINE_STATISTICS;

    for (int i = 0; i < 10; i++)
    {
        if (FAILED(pDevice->CreateQuery(&queryDesc, &m_pPipelineStatsQuery[i])))
        {
            return E_FAIL;
        }
    }
    return S_OK;
}

void ReadQueries(ID3D11DeviceContext* pDeviceContext)
{
    D3D11_QUERY_DATA_PIPELINE_STATISTICS stats;

    while (m_lastCompletedQueryFrame + QUERY_FRAME_DELAY < m_curQueryFrame)
    {
        HRESULT hr = pDeviceContext->GetData(
            m_pPipelineStatsQuery[m_lastCompletedQueryFrame % 10],
            &stats,
            sizeof(stats),
            0);

        if (hr == S_OK)
        {
            m_gpuVisibleInstances = static_cast<int>(stats.IAPrimitives) / 12;
            m_lastCompletedQueryFrame++;
        }
        else
        {
            break;
        }
    }
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

void ReleasePostProcessResources() {
    if (m_pColorBuffer) m_pColorBuffer->Release();
    if (m_pColorBufferRTV) m_pColorBufferRTV->Release();
    if (m_pColorBufferSRV) m_pColorBufferSRV->Release();
    if (m_pPostProcessVS) m_pPostProcessVS->Release();
    if (m_pPostProcessPS) m_pPostProcessPS->Release();
    m_pColorBuffer = nullptr;
    m_pColorBufferRTV = nullptr;
    m_pColorBufferSRV = nullptr;
    m_pPostProcessVS = nullptr;
    m_pPostProcessPS = nullptr;
}

HRESULT CreatePostProcessResources(ID3D11Device* pDevice) {
    ReleasePostProcessResources();

    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width = m_width;
    texDesc.Height = m_height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = pDevice->CreateTexture2D(&texDesc, nullptr, &m_pColorBuffer);
    if (FAILED(hr)) return hr;

    hr = pDevice->CreateRenderTargetView(m_pColorBuffer, nullptr, &m_pColorBufferRTV);
    if (FAILED(hr)) return hr;

    hr = pDevice->CreateShaderResourceView(m_pColorBuffer, nullptr, &m_pColorBufferSRV);
    if (FAILED(hr)) return hr;

    const char* postProcessVSCode = R"(
    struct VSOutput {
        float4 pos : SV_Position;
        float2 uv : TEXCOORD;
    };

    VSOutput main(uint id : SV_VertexID) {
        VSOutput output;
        output.uv = float2((id << 1) & 2, id & 2);
        output.pos = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
        return output;
    })";

    const char* postProcessPSCode = R"(
    Texture2D sceneTexture : register(t0);
    SamplerState samplerState : register(s0);

    float3 ApplySepia(float3 color) {
        float3 sepia = float3(
           dot(color, float3(0.393, 0.769, 0.189)),
           dot(color, float3(0.349, 0.686, 0.168)),
           dot(color, float3(0.272, 0.534, 0.131))
        );
    
        float3 original = color;
        float gray = dot(original, float3(0.299, 0.587, 0.114));
        float3 blended = lerp(gray, sepia, 0.75);
    
        return saturate(blended * 1.1);
    }

    float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target0 {
        float3 color = sceneTexture.Sample(samplerState, uv).rgb;
        float3 sepiaColor = ApplySepia(color);
        return float4(sepiaColor, 1.0);
    })";

    ID3DBlob* pVSBlob = nullptr;
    hr = D3DCompile(postProcessVSCode, strlen(postProcessVSCode), nullptr, nullptr, nullptr,
        "main", "vs_5_0", 0, 0, &pVSBlob, nullptr);
    if (SUCCEEDED(hr)) {
        hr = pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pPostProcessVS);
        pVSBlob->Release();
    }
    if (FAILED(hr)) return hr;

    ID3DBlob* pPSBlob = nullptr;
    hr = D3DCompile(postProcessPSCode, strlen(postProcessPSCode), nullptr, nullptr, nullptr,
        "main", "ps_5_0", 0, 0, &pPSBlob, nullptr);
    if (SUCCEEDED(hr)) {
        hr = pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pPostProcessPS);
        pPSBlob->Release();
    }

    return hr;
}

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

HRESULT CreateInputLayout(ID3D11Device* pDevice, ID3D11InputLayout** ppInputLayout, ID3DBlob* pVertexShaderCode) {
    D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXINDEX", 0, DXGI_FORMAT_R32_UINT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"USENORMALMAP", 0, DXGI_FORMAT_R32_UINT, 1, 4, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"ORIGINAL_ID", 0, DXGI_FORMAT_R32_UINT, 1, 8, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"INSTANCEID", 0, DXGI_FORMAT_R32_UINT, 1, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1} };
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

// Глобальные переменные для доступа из WndProc
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
static ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
static ID3D11Texture2D* g_pDepthBuffer = nullptr;
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pDeviceContext = nullptr;  // Добавляем это

void Render(
    ID3D11DeviceContext* pDeviceContext,
    ID3D11Buffer* pIndexBuffer,
    ID3D11Buffer* pVertexBuffer,
    ID3D11InputLayout* pInputLayout,
    ID3D11VertexShader* pVertexShader,
    ID3D11PixelShader* pPixelShader,
    ID3D11Buffer* pGeomBufferInst,
    ID3D11Buffer* pInstanceBuffer,
    ID3D11SamplerState* pSampler,
    ID3D11ShaderResourceView* pTextureView,
    ID3D11Buffer* pSphereIndexBuffer,
    ID3D11Buffer* pSphereVertexBuffer,
    ID3D11InputLayout* pSphereInputLayout,
    ID3D11VertexShader* pSphereVertexShader,
    ID3D11PixelShader* pSpherePixelShader,
    ID3D11Buffer* pSphereGeomBuffer,
    ID3D11Buffer* pSphereSceneBuffer,
    ID3D11ShaderResourceView* pSphereTextureView,
    ID3D11Buffer* pSquareVertexBuffer,
    ID3D11Buffer* pSquareIndexBuffer,
    ID3D11InputLayout* pSquareInputLayout,
    ID3D11VertexShader* pSquareVertexShader,
    ID3D11PixelShader* pSquarePixelShader,
    ID3D11Buffer* pSquareGeomBuffer,
    ID3D11Buffer* pColorBuffer,
    ID3D11RasterizerState* pNoCullRasterizerState,
    ID3D11BlendState* pTransBlendState,
    ID3D11DepthStencilState* pNoWriteDepthStencilState,
    const DirectX::XMFLOAT3& cameraPosition,
    ID3D11Buffer* pSceneBuffer,
    ID3D11Buffer* pMaterialBuffer,
    ID3D11Buffer* pLightGeomBuffer,
    ID3D11PixelShader* pLightPixelShader,
    ID3D11ShaderResourceView* pTextureNormalView,
    ID3D11Buffer* pVisibleInstancesBuffer,
    UINT visibleCount,
    ID3D11ComputeShader* pFrustumCullingCS,
    ID3D11Buffer* pIndirectArgsSrc,
    ID3D11Buffer* pIndirectArgs,
    ID3D11UnorderedAccessView* pIndirectArgsUAV,
    ID3D11Buffer* pVisibleObjectsBuffer,
    ID3D11UnorderedAccessView* pVisibleObjectsUAV,
    ID3D11ShaderResourceView* pVisibleObjectsSRV,
    ID3D11Buffer* pAABBBuffer,
    ID3D11Buffer* pFrustumBuffer,
    bool useGPUCulling)
{
    static const FLOAT clearColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
    pDeviceContext->ClearRenderTargetView(m_pColorBufferRTV, clearColor);
    pDeviceContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    pDeviceContext->OMSetRenderTargets(1, &m_pColorBufferRTV, g_pDepthStencilView);
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (float)m_width;
    viewport.Height = (float)m_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    pDeviceContext->RSSetViewports(1, &viewport);
    pDeviceContext->PSSetConstantBuffers(1, 1, &pSceneBuffer);
    pDeviceContext->PSSetConstantBuffers(2, 1, &pMaterialBuffer);

    // Отрисовка сферы (небо)
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

    // Отрисовка кубов
    pDeviceContext->PSSetShaderResources(1, 1, &pTextureNormalView);
    pDeviceContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { pVertexBuffer, pInstanceBuffer };
    UINT strides[] = { sizeof(TextureTangentVertex), sizeof(InstanceData) };
    UINT offsets[] = { 0, 0 };
    pDeviceContext->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);
    pDeviceContext->IASetInputLayout(pInputLayout);
    pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pDeviceContext->VSSetShader(pVertexShader, nullptr, 0);
    pDeviceContext->PSSetShader(pPixelShader, nullptr, 0);
    pDeviceContext->PSSetSamplers(0, 1, &pSampler);
    pDeviceContext->VSSetConstantBuffers(1, 1, &pGeomBufferInst);
    pDeviceContext->PSSetShaderResources(0, 1, &pTextureView);

    if (useGPUCulling) {
        D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS args;
        args.IndexCountPerInstance = 36;
        args.InstanceCount = 0;
        args.StartIndexLocation = 0;
        args.BaseVertexLocation = 0;
        args.StartInstanceLocation = 0;

        pDeviceContext->UpdateSubresource(pIndirectArgsSrc, 0, nullptr, &args, 0, 0);
        UINT groupNumber = (MaxInst + 63) / 64;

        ID3D11Buffer* constBuffers[3] = { pSceneBuffer, pAABBBuffer, pFrustumBuffer };
        pDeviceContext->CSSetConstantBuffers(0, 3, constBuffers);

        ID3D11UnorderedAccessView* uavBuffers[2] = { pIndirectArgsUAV, pVisibleObjectsUAV };
        UINT initialCounts[2] = { 0, 0 };
        pDeviceContext->CSSetUnorderedAccessViews(0, 2, uavBuffers, nullptr);

        ID3D11ShaderResourceView* srvs[1] = { pVisibleObjectsSRV };
        pDeviceContext->VSSetShaderResources(2, 1, srvs); // t2 в шейдере

        pDeviceContext->CSSetShader(pFrustumCullingCS, nullptr, 0);

        pDeviceContext->Dispatch(groupNumber, 1, 1);

        // Копирование результатов
        pDeviceContext->CopyResource(pVisibleInstancesBuffer, pVisibleObjectsBuffer);
        pDeviceContext->CopyResource(pIndirectArgs, pIndirectArgsSrc);

        // Начинаем запрос статистики
        pDeviceContext->Begin(m_pPipelineStatsQuery[m_curQueryFrame % 10]);

        // Отрисовка с использованием indirect
        pDeviceContext->DrawIndexedInstancedIndirect(pIndirectArgs, 0);

        // Заканчиваем запрос
        pDeviceContext->End(m_pPipelineStatsQuery[m_curQueryFrame % 10]);
        m_curQueryFrame++;
    }
    else {
        // Старый способ отрисовки
        pDeviceContext->VSSetConstantBuffers(2, 1, &pVisibleInstancesBuffer);
        pDeviceContext->DrawIndexedInstanced(36, visibleCount, 0, 0, 0);
    }

    ReadQueries(pDeviceContext);

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
    pDeviceContext->OMSetDepthStencilState(nullptr, 0);

    // постпроцессинг
    pDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);


    ID3D11SamplerState* samplers[] = { pSampler };
    pDeviceContext->PSSetSamplers(0, 1, samplers);

    ID3D11ShaderResourceView* resources[] = { m_pColorBufferSRV };
    pDeviceContext->PSSetShaderResources(0, 1, resources);

    pDeviceContext->OMSetDepthStencilState(nullptr, 0);

    pDeviceContext->IASetInputLayout(nullptr);
    pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pDeviceContext->VSSetShader(m_pPostProcessVS, nullptr, 0);
    pDeviceContext->PSSetShader(m_pPostProcessPS, nullptr, 0);

    pDeviceContext->Draw(6, 0);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    switch (message) {
    case WM_CLOSE:
        PostQuitMessage(0);
        break;

    case WM_SIZE:
    {
        UINT newWidth = LOWORD(lParam);
        UINT newHeight = HIWORD(lParam);
        if (newWidth < 64) newWidth = 64;
        if (newHeight < 64) newHeight = 64;

        m_width = newWidth;
        m_height = newHeight;

        if (g_pDeviceContext)
            g_pDeviceContext->ClearState();

        if (g_pRenderTargetView) { g_pRenderTargetView->Release(); g_pRenderTargetView = nullptr; }
        if (g_pDepthStencilView) { g_pDepthStencilView->Release(); g_pDepthStencilView = nullptr; }
        if (g_pDepthBuffer) { g_pDepthBuffer->Release(); g_pDepthBuffer = nullptr; }
        ReleasePostProcessResources();

        if (g_pSwapChain)
        {
            HRESULT hr = g_pSwapChain->ResizeBuffers(
                1,
                m_width,
                m_height,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                0);

            if (FAILED(hr))
            {
                break;
            }

            ID3D11Texture2D* pBackBuffer = nullptr;
            hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
            if (SUCCEEDED(hr))
            {
                hr = g_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
                pBackBuffer->Release();
            }

            if (SUCCEEDED(hr))
            {
                D3D11_TEXTURE2D_DESC depthDesc = {};
                depthDesc.Width = m_width;
                depthDesc.Height = m_height;
                depthDesc.MipLevels = 1;
                depthDesc.ArraySize = 1;
                depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
                depthDesc.SampleDesc.Count = 1;
                depthDesc.SampleDesc.Quality = 0;
                depthDesc.Usage = D3D11_USAGE_DEFAULT;
                depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

                hr = g_pDevice->CreateTexture2D(&depthDesc, nullptr, &g_pDepthBuffer);
            }

            if (SUCCEEDED(hr))
            {
                D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc = {};
                depthViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
                depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                depthViewDesc.Texture2D.MipSlice = 0;

                hr = g_pDevice->CreateDepthStencilView(g_pDepthBuffer, &depthViewDesc, &g_pDepthStencilView);
            }

            if (SUCCEEDED(hr))
                hr = CreatePostProcessResources(g_pDevice);

            if (SUCCEEDED(hr))
            {
                D3D11_VIEWPORT viewport = {};
                viewport.Width = (float)m_width;
                viewport.Height = (float)m_height;
                viewport.MinDepth = 0.0f;
                viewport.MaxDepth = 1.0f;
                g_pDeviceContext->RSSetViewports(1, &viewport);
            }
        }
        break;
    }

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


HWND CreateWindowInstance(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"Lab8WindowClass";
    RegisterClassEx(&wcex);

    RECT rect = { 0, 0, m_width, m_height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindow(wcex.lpszClassName, L"Lab8", WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    SendMessage(hWnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(m_width, m_height));

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
    deptStateDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    deptStateDesc.StencilEnable = FALSE;

    hr = (*ppDevice)->CreateDepthStencilState(&deptStateDesc, ppDepthState);
    if (FAILED(hr)) {
        return hr;
    }

    g_pSwapChain = *ppSwapChain;
    g_pRenderTargetView = *ppRenderTargetView;
    g_pDepthStencilView = *ppDepthStencilView;
    g_pDepthBuffer = *ppDepthBuffer;
    g_pDevice = *ppDevice;
    g_pDeviceContext = *ppDeviceContext;

    hr = InitQueries(*ppDevice);


    return hr;
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
    if (GetAsyncKeyState('W') & 0x8000 && cameraRadius > 0.5) {
        cameraRadius -= rotationSpeed * 2.0 * deltaTime;
    }
    // Уменьшение расстояния
    if (GetAsyncKeyState('S') & 0x8000 && cameraRadius < 100.0) {
        cameraRadius += rotationSpeed * 2.0 * deltaTime;
    }
}

void UpdateRotation(double deltaTime,
    ID3D11DeviceContext* pDeviceContext,
    ID3D11Buffer* pGeomBufferInst,
    ID3D11Buffer* pLightGeomBuffer,
    ID3D11Buffer* pSphereGeomBuffer,
    ID3D11Buffer* pSphereSceneBuffer,
    ID3D11Buffer* pSquareGeomBuffer,
    double& angle_y,
    double& angle_xz,
    double& cameraRadius,
    DirectX::XMFLOAT3& cameraPosition,
    ID3D11Buffer* pSceneBuffer,
    ID3D11Buffer* pVisibleInstancesBuffer,
    UINT& visibleCount,
    ID3D11Buffer* pInstanceBuffer,
    InstanceData* instanceData,
    ID3D11Buffer* pAABBBuffer,
    ID3D11Buffer* pFrustumBuffer,
    bool useGPUCulling)
{
    static const double rotationViewSpeed = 1.3;
    HandleInput(deltaTime, angle_y, angle_xz, rotationViewSpeed, cameraRadius);

    GeomBuffer geomBuffers[MaxInst];
    GeomBuffer geomLightBuffer;
    GeomBuffer sphereGeomBuffer;
    SceneBuffer sphereSceneBuffer;

    SceneBuffer sceneBuffer = {};
    sceneBuffer.lightCount = DirectX::XMFLOAT4(1, 0, 0, 0);
    sceneBuffer.lights[0].pos = DirectX::XMFLOAT4(0.5f, 0.7f, -0.5f, 1.0f);
    sceneBuffer.lights[0].color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    sceneBuffer.ambientColor = DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);

    float cameraX = cameraRadius * sinf(static_cast<float>(angle_y));
    float cameraZ = cameraRadius * cosf(static_cast<float>(angle_y));
    float cameraY = cameraRadius * sinf(static_cast<float>(angle_xz));
    cameraX *= cosf(static_cast<float>(angle_xz));
    cameraZ *= cosf(static_cast<float>(angle_xz));

    cameraPosition = DirectX::XMFLOAT3(cameraX, cameraY, cameraZ);
    sceneBuffer.cameraPos = DirectX::XMFLOAT4(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f);

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(
        DirectX::XMVectorSet(cameraX, cameraY, cameraZ, 0.0f),
        DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
    );

    DirectX::XMMATRIX offset = DirectX::XMMatrixTranslation(0.0f, 0.0f, 1.0f);

    DirectX::XMMATRIX viewOffset = DirectX::XMMatrixMultiply(view, offset);

    float fov = DirectX::XM_PI / 3.0f;
    float aspectRatio = 1280.0f / 720.0f;
    float nearZ = 0.1f;
    float farZ = 1000.0f;
    auto proj = DirectX::XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);

    DirectX::XMMATRIX viewProj = XMMatrixMultiply(viewOffset, proj);
    DirectX::XMFLOAT4X4 viewProjMatrix;
    XMStoreFloat4x4(&viewProjMatrix, viewProj);

    DirectX::XMFLOAT4 frustumPlanes[6];

    // Near plane
    frustumPlanes[0] = DirectX::XMFLOAT4(
        viewProjMatrix._13, viewProjMatrix._23, viewProjMatrix._33, viewProjMatrix._43
    );
    // Far plane
    frustumPlanes[1] = DirectX::XMFLOAT4(
        viewProjMatrix._14 - viewProjMatrix._13,
        viewProjMatrix._24 - viewProjMatrix._23,
        viewProjMatrix._34 - viewProjMatrix._33,
        viewProjMatrix._44 - viewProjMatrix._43
    );
    // Left plane
    frustumPlanes[2] = DirectX::XMFLOAT4(
        viewProjMatrix._14 + viewProjMatrix._11,
        viewProjMatrix._24 + viewProjMatrix._21,
        viewProjMatrix._34 + viewProjMatrix._31,
        viewProjMatrix._44 + viewProjMatrix._41
    );
    // Right plane
    frustumPlanes[3] = DirectX::XMFLOAT4(
        viewProjMatrix._14 - viewProjMatrix._11,
        viewProjMatrix._24 - viewProjMatrix._21,
        viewProjMatrix._34 - viewProjMatrix._31,
        viewProjMatrix._44 - viewProjMatrix._41
    );
    // Top plane
    frustumPlanes[4] = DirectX::XMFLOAT4(
        viewProjMatrix._14 - viewProjMatrix._12,
        viewProjMatrix._24 - viewProjMatrix._22,
        viewProjMatrix._34 - viewProjMatrix._32,
        viewProjMatrix._44 - viewProjMatrix._42
    );
    // Bottom plane
    frustumPlanes[5] = DirectX::XMFLOAT4(
        viewProjMatrix._14 + viewProjMatrix._12,
        viewProjMatrix._24 + viewProjMatrix._22,
        viewProjMatrix._34 + viewProjMatrix._32,
        viewProjMatrix._44 + viewProjMatrix._42
    );

    for (int i = 0; i < 6; i++) {
        DirectX::XMVECTOR planeVec = DirectX::XMPlaneNormalize(DirectX::XMLoadFloat4(&frustumPlanes[i]));
        DirectX::XMStoreFloat4(&frustumPlanes[i], planeVec);
    }

    Point4f frustumPls[6];
    for (size_t i = 0; i < 6; i++)
    {
        frustumPls[i] = Point4f(frustumPlanes[i].x, frustumPlanes[i].y, frustumPlanes[i].z, frustumPlanes[i].w);
    }

    GeomBufferInstVis visibleInstances = {};
    visibleCount = 0;

    Point3f cubeMin = { -0.5f, -0.5f, -0.5f };
    Point3f cubeMax = { 0.5f, 0.5f, 0.5f };

    DirectX::XMVECTOR rotationAxis = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
    rotationAxis = DirectX::XMVector3Normalize(rotationAxis);
    static float rotationAngle = 0.0f;
    rotationAngle += 0.05f * static_cast<float>(deltaTime);
    if (rotationAngle > 2 * DirectX::XM_PI) {
        rotationAngle -= 2 * DirectX::XM_PI;
    }
    DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationAxis(rotationAxis, rotationAngle);

    // Центральный куб (всегда видим)
    geomBuffers[0].model = rotationMatrix;
    visibleInstances.ids[visibleCount++].x = 0;

    // Остальные кубы по кругу
    float radius = 3.0f;
    for (int i = 1; i < MaxInst; i++) {
        float angle = DirectX::XM_2PI * (i - 1) / (MaxInst - 1);
        float x = radius * (i % 2 == 0 ? 2 : 1) * cosf(angle);
        float z = radius * (i % 2 == 0 ? 2 : 1) * sinf(angle);

        geomBuffers[i].model = DirectX::XMMatrixTranslation(x, 0.0f, z);

        Point3f worldMin, worldMax;
        TransformAABB(cubeMin, cubeMax, geomBuffers[i].model, worldMin, worldMax);

        // Проверяем видимость
        if (IsBoxInside(frustumPls, worldMin, worldMax)) {
            visibleInstances.ids[visibleCount++].x = i;
        }
    }

    if (pVisibleInstancesBuffer) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(pDeviceContext->Map(pVisibleInstancesBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, &visibleInstances, sizeof(GeomBufferInstVis));
            pDeviceContext->Unmap(pVisibleInstancesBuffer, 0);
        }
    }

    InstanceData visibleInstanceData[MaxInst];
    for (UINT i = 0; i < visibleCount; i++) {
        UINT originalId = visibleInstances.ids[i].x;
        visibleInstanceData[i] = instanceData[originalId];
    }

    D3D11_MAPPED_SUBRESOURCE mappedInst;
    if (SUCCEEDED(pDeviceContext->Map(pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedInst))) {
        memcpy(mappedInst.pData, visibleInstanceData, sizeof(InstanceData) * visibleCount);
        pDeviceContext->Unmap(pInstanceBuffer, 0);
    }

    for (int i = 0; i < MaxInst; i++) {
        geomBuffers[i].view = DirectX::XMMatrixMultiply(view, offset);
        geomBuffers[i].projection = proj;
        geomBuffers[i].normalMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, geomBuffers[i].model));
    }

    pDeviceContext->UpdateSubresource(pSceneBuffer, 0, nullptr, &sceneBuffer, 0, 0);

    pDeviceContext->UpdateSubresource(pGeomBufferInst, 0, nullptr, &geomBuffers, 0, 0);

    geomLightBuffer.model = DirectX::XMMatrixScaling(0.1f, 0.1f, 0.1f) *
        DirectX::XMMatrixTranslation(sceneBuffer.lights[0].pos.x, sceneBuffer.lights[0].pos.y, sceneBuffer.lights[0].pos.z);
    geomLightBuffer.view = DirectX::XMMatrixMultiply(view, offset);
    geomLightBuffer.projection = proj;
    pDeviceContext->UpdateSubresource(pLightGeomBuffer, 0, nullptr, &geomLightBuffer, 0, 0);

    sphereGeomBuffer.model = DirectX::XMMatrixIdentity();
    pDeviceContext->UpdateSubresource(pSphereGeomBuffer, 0, nullptr, &sphereGeomBuffer, 0, 0);

    sphereSceneBuffer.vp = geomBuffers[0].view * geomBuffers[0].projection;
    sphereSceneBuffer.cameraPos.x = cameraX;
    sphereSceneBuffer.cameraPos.y = cameraY;
    sphereSceneBuffer.cameraPos.z = cameraZ;
    pDeviceContext->UpdateSubresource(pSphereSceneBuffer, 0, nullptr, &sphereSceneBuffer, 0, 0);

    GeomBuffer squareGeomBuffer;
    squareGeomBuffer.model = DirectX::XMMatrixIdentity();
    squareGeomBuffer.view = DirectX::XMMatrixMultiply(view, offset);
    squareGeomBuffer.projection = proj;
    squareGeomBuffer.normalMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, squareGeomBuffer.model));
    pDeviceContext->UpdateSubresource(pSquareGeomBuffer, 0, nullptr, &squareGeomBuffer, 0, 0);

    // для GPU frustum culling
    if (pFrustumBuffer) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(pDeviceContext->Map(pFrustumBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            FrustumBuffer* pData = (FrustumBuffer*)mapped.pData;
            for (int i = 0; i < 6; i++) {
                pData->planes[i] = frustumPlanes[i];
            }
            pDeviceContext->Unmap(pFrustumBuffer, 0);
        }
    }

    if (pAABBBuffer) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(pDeviceContext->Map(pAABBBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            AABBData* pData = (AABBData*)mapped.pData;
            pData->numShapes.x = MaxInst;

            for (int i = 0; i < MaxInst; i++) {
                Point3f worldMin, worldMax;
                TransformAABB(cubeMin, cubeMax, geomBuffers[i].model, worldMin, worldMax);

                pData->bbMin[i] = DirectX::XMFLOAT4(worldMin.x, worldMin.y, worldMin.z, 1.0f);
                pData->bbMax[i] = DirectX::XMFLOAT4(worldMax.x, worldMax.y, worldMax.z, 1.0f);
            }
            pDeviceContext->Unmap(pAABBBuffer, 0);
        }
    }
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

    // интерфейс
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(g_pDevice, g_pDeviceContext);
    CreatePostProcessResources(pDevice);

    
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
    TextureDesc textureDescs[2]; // Массив для двух текстур
    const std::wstring textureNames[2] = { L"texture.dds", L"CAT.dds" };

    for (int i = 0; i < 2; i++) {
        if (!LoadDDS(textureNames[i], textureDescs[i])) {
            return -1;
        }
    }

    if (textureDescs[0].fmt != textureDescs[1].fmt ||
        textureDescs[0].width != textureDescs[1].width ||
        textureDescs[0].height != textureDescs[1].height) {
        return -1;
    }

    // создание текстуры
    ID3D11Texture2D* pTexture = nullptr;
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = textureDescs[0].width;
    texDesc.Height = textureDescs[0].height;
    texDesc.MipLevels = textureDescs[0].mipmapsCount;
    texDesc.ArraySize = 2; // Две текстуры в массиве
    texDesc.Format = textureDescs[0].fmt;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    std::vector<D3D11_SUBRESOURCE_DATA> subresourceData;
    subresourceData.resize(texDesc.MipLevels * 2);

    for (UINT j = 0; j < 2; j++) {
        UINT32 blockWidth = DivUp(texDesc.Width, 4u);
        UINT32 blockHeight = DivUp(texDesc.Height, 4u);
        UINT32 pitch = blockWidth * GetBytesPerBlock(texDesc.Format);

        const char* pSrcData = reinterpret_cast<const char*>(textureDescs[j].pData);

        for (UINT i = 0; i < texDesc.MipLevels; i++) {
            subresourceData[j * texDesc.MipLevels + i].pSysMem = pSrcData;
            subresourceData[j * texDesc.MipLevels + i].SysMemPitch = pitch;
            subresourceData[j * texDesc.MipLevels + i].SysMemSlicePitch = 0;

            pSrcData += pitch * blockHeight;
            blockWidth = (blockWidth / 2) > 1 ? (blockWidth / 2) : 1;
            blockHeight = (blockHeight / 2) > 1 ? (blockHeight / 2) : 1;
            pitch = blockWidth * GetBytesPerBlock(texDesc.Format);
        }
    }

    hr = pDevice->CreateTexture2D(&texDesc, subresourceData.data(), &pTexture);
    if (FAILED(hr)) {
        return -1;
    }

    ID3D11ShaderResourceView* pTextureView = nullptr;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDescs[0].fmt;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = texDesc.MipLevels;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = 2; // Все две текстуры

    hr = pDevice->CreateShaderResourceView(pTexture, &srvDesc, &pTextureView);
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
    ID3D11Buffer* pGeomBufferInst = nullptr;

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(GeomBuffer) * MaxInst;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    hr = pDevice->CreateBuffer(&desc, nullptr, &pGeomBufferInst);
    if (FAILED(hr)) {
        return -1;
    }

    ID3D11Buffer* pVisibleInstancesBuffer = nullptr;
    D3D11_BUFFER_DESC visDesc = {};
    visDesc.ByteWidth = sizeof(GeomBufferInstVis);
    visDesc.Usage = D3D11_USAGE_DYNAMIC;
    visDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    visDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = pDevice->CreateBuffer(&visDesc, nullptr, &pVisibleInstancesBuffer);
    if (FAILED(hr))
    {
        return -1;
    }

    ID3D11Buffer* pInstanceBuffer = nullptr;
    D3D11_BUFFER_DESC instDesc = {};
    instDesc.ByteWidth = sizeof(InstanceData) * MaxInst;
    instDesc.Usage = D3D11_USAGE_DYNAMIC;
    instDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    instDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    InstanceData instanceData[10] = {
        { 0, TRUE, 0,0},
        { 1, FALSE, 1,0},
        { 0, TRUE, 2,0},
        { 1, FALSE, 3,0},
        { 0, TRUE, 4,0},
        { 0, TRUE, 5,0},
        { 1, FALSE, 6,0},
        { 0, TRUE, 7,0},
        { 1, FALSE, 8,0},
        { 0, TRUE, 9,0}
    };

    D3D11_SUBRESOURCE_DATA instData = {};
    instData.pSysMem = instanceData;
    hr = pDevice->CreateBuffer(&instDesc, &instData, &pInstanceBuffer);
    if (FAILED(hr))
    {
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

    // Создание буфера для indirect аргументов
    ID3D11Buffer* pIndirectArgsSrc = nullptr;
    ID3D11UnorderedAccessView* pIndirectArgsUAV = nullptr;
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = sizeof(UINT);

        D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS initArgs = {};
        initArgs.IndexCountPerInstance = 36;
        initArgs.InstanceCount = 0;  // Важно инициализировать нулем!
        initArgs.StartIndexLocation = 0;
        initArgs.BaseVertexLocation = 0;
        initArgs.StartInstanceLocation = 0;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = &initArgs;

        HRESULT hr = pDevice->CreateBuffer(&desc, nullptr, &pIndirectArgsSrc);
        if (FAILED(hr)) {
            MessageBox(hWnd, L"Failed to create indirect args buffer", L"Error", MB_OK);
            return -1;
        }

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = sizeof(D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS) / sizeof(UINT);

        hr = pDevice->CreateUnorderedAccessView(pIndirectArgsSrc, nullptr, &pIndirectArgsUAV);
        if (FAILED(hr)) {
            MessageBox(hWnd, L"Failed to create indirect args UAV", L"Error", MB_OK);
            return -1;
        }
    }

    // Буфер для indirect аргументов (копия)
    ID3D11Buffer* m_pIndirectArgs = nullptr;
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
        desc.StructureByteStride = 0;

        HRESULT result = pDevice->CreateBuffer(&desc, nullptr, &m_pIndirectArgs);
        if (FAILED(result)) {
            return -1;
        }
    }

    // Буфер для AABB данных
    ID3D11Buffer* m_pAABBBuffer = nullptr;
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(DirectX::XMFLOAT4) * 2 * MaxInst; // bbMin и bbMax для каждого объекта
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;

        HRESULT result = pDevice->CreateBuffer(&desc, nullptr, &m_pAABBBuffer);
        if (FAILED(result)) {
            return -1;
        }
    }

    // Буфер для ID видимых объектов (UAV)
    ID3D11Buffer* m_pVisibleObjectsBuffer = nullptr;
    ID3D11UnorderedAccessView* m_pVisibleObjectsUAV = nullptr;
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(DirectX::XMUINT4) * MaxInst;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = sizeof(DirectX::XMUINT4);

        HRESULT result = pDevice->CreateBuffer(&desc, nullptr, &m_pVisibleObjectsBuffer);
        if (SUCCEEDED(result)) {
            result = pDevice->CreateUnorderedAccessView(m_pVisibleObjectsBuffer, nullptr, &m_pVisibleObjectsUAV);
            if (FAILED(result)) {
                return -1;
            }
        }
        else {
            return -1;
        }
    }

    ID3D11ShaderResourceView* pVisibleObjectsSRV = nullptr;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc12 = {};
    srvDesc12.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc12.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc12.Buffer.FirstElement = 0;
    srvDesc12.Buffer.NumElements = MaxInst;

    hr = pDevice->CreateShaderResourceView(
        m_pVisibleObjectsBuffer,
        &srvDesc12,
        &pVisibleObjectsSRV);

    // Буфер для параметров фрустума
    ID3D11Buffer* m_pFrustumBuffer = nullptr;
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(DirectX::XMFLOAT4) * 6; // 6 плоскостей
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;

        HRESULT result = pDevice->CreateBuffer(&desc, nullptr, &m_pFrustumBuffer);
        if (FAILED(result)) {
            return -1;
        }
    }

    // Компиляция compute shader
    ID3D11ComputeShader* m_pFrustumCullingCS = nullptr;
    {
        ID3DBlob* pCSBlob = nullptr;
        HRESULT result = CompileShader(computeShaderCode, "cs", "cs_5_0", &pCSBlob);
        if (SUCCEEDED(result))
        {
            result = pDevice->CreateComputeShader(
                pCSBlob->GetBufferPointer(),
                pCSBlob->GetBufferSize(),
                nullptr,
                &m_pFrustumCullingCS);
            pCSBlob->Release();

            if (FAILED(result))
            {
                return -1;
            }
        }
        else
        {
            return -1;
        }
    }

    ID3D11Buffer* pAABBBuffer = nullptr;
    ID3D11Buffer* pFrustumBuffer = nullptr;
    // Создание буфера фрустума
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(DirectX::XMFLOAT4) * 6;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = pDevice->CreateBuffer(&desc, nullptr, &pFrustumBuffer);
        if (FAILED(hr)) {
            MessageBox(hWnd, L"Failed to create frustum buffer", L"Error", MB_OK);
            return -1;
        }
    }

    // Создание буфера AABB
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(DirectX::XMFLOAT4) * 2 * MaxInst; // bbMin и bbMax для каждого объекта
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = pDevice->CreateBuffer(&desc, nullptr, &pAABBBuffer);
        if (FAILED(hr)) {
            MessageBox(hWnd, L"Failed to create AABB buffer", L"Error", MB_OK);
            return -1;
        }
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

            UINT visibleCount = 0;

            UpdateRotation(elapsed.count(), pDeviceContext,
                pGeomBufferInst, pLightGeomBuffer, pSphereGeomBuffer,
                pSphereSceneBuffer, pSquareGeomBuffer, angle_y, angle_xz,
                cameraRadius, cameraPosition, pSceneBuffer,
                pVisibleInstancesBuffer, visibleCount, pInstanceBuffer,
                instanceData, pAABBBuffer, pFrustumBuffer, useGPUCulling);

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            ImGui::Begin("Control Panel");
            ImGui::TextColored(useGPUCulling ? ImVec4(1, 1, 1, 1) : ImVec4(0, 1, 0, 1), "CPU Visible Instances: %d", visibleCount);
            ImGui::TextColored(useGPUCulling ? ImVec4(0, 1, 0, 1) : ImVec4(1, 1, 1, 1), "GPU Visible Instances: %d", m_gpuVisibleInstances);
            ImGui::Text("(Delayed by %d frames)", QUERY_FRAME_DELAY);
            ImGui::Checkbox("GPU Frustum Culling", &useGPUCulling);
            ImGui::Text("Controls:");
            ImGui::BulletText("W/S - Camera radius");
            ImGui::BulletText("VK_LEFT/VK_RIGHT - Horizontal rotation");
            ImGui::BulletText("VK_UP/VK_DOWN - Vertical rotation");
            ImGui::End();

            Render(pDeviceContext, pIndexBuffer, pVertexBuffer,
                pInputLayout, pVertexShader, pPixelShader,
                pGeomBufferInst, pInstanceBuffer, pSampler,
                pTextureView, pSphereIndexBuffer, pSphereVertexBuffer,
                pSphereInputLayout, pSphereVertexShader, pSpherePixelShader,
                pSphereGeomBuffer, pSphereSceneBuffer, pSphereTextureView,
                pSquareVertexBuffer, pSquareIndexBuffer, pSquareInputLayout,
                pSquareVertexShader, pSquarePixelShader, pSquareGeomBuffer,
                pColorBuffer, pNoCullRasterizerState, pTransBlendState,
                pNoWriteDepthStencilState, cameraPosition, pSceneBuffer,
                pMaterialBuffer, pLightGeomBuffer, pLightPixelShader,
                pTextureNormalView, pVisibleInstancesBuffer, visibleCount,
                m_pFrustumCullingCS, pIndirectArgsSrc, m_pIndirectArgs,
                pIndirectArgsUAV, m_pVisibleObjectsBuffer, m_pVisibleObjectsUAV, pVisibleObjectsSRV, pAABBBuffer,
                pFrustumBuffer, useGPUCulling);

            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            pSwapChain->Present(1, 0);
        }
    }

    // Освобождение ресурсов
    if (pVertexBuffer) pVertexBuffer->Release();
    if (pIndexBuffer) pIndexBuffer->Release();
    if (pVertexShader) pVertexShader->Release();
    if (pPixelShader) pPixelShader->Release();
    if (pInputLayout) pInputLayout->Release();
    if (pSwapChain) pSwapChain->Release();
    if (pDeviceContext) pDeviceContext->Release();
    if (pDevice) pDevice->Release();
    if (pGeomBuffer) pGeomBuffer->Release();
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
    if (pGeomBufferInst) pGeomBufferInst->Release();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    return (int)msg.wParam;
}