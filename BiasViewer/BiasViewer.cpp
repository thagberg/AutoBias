// BiasViewer.cpp : Defines the entry point for the application.
//

#define NOMINMAX 1

#include "framework.h"
#include "BiasViewer.h"

#include <vector>
#include <iostream>
#include <cmath>
#include <limits>
#include <algorithm>
#include <assert.h>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>

#include <ArduinoControl.h>

#include "desktopcapture.h"
#include "CaptureManager.h"

#pragma comment(lib, "d3d11.lib")

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int, HWND& window);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

const uint8_t kNumVertices = 6;

struct DownsampleConstantBuffer
{
    uint32_t dispatchX;
    uint32_t dispatchY;
};

struct Vertex
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 TexCoord;
};

const Vertex kVertices[kNumVertices] =
{
	{DirectX::XMFLOAT3(-1.f, -1.f, 0.f), DirectX::XMFLOAT2(0.f, 1.f)},
	{DirectX::XMFLOAT3(-1.f, 1.f, 0.f), DirectX::XMFLOAT2(0.f, 0.f)},
	{DirectX::XMFLOAT3(1.f, -1.f, 0.f), DirectX::XMFLOAT2(1.f, 1.f)},
	{DirectX::XMFLOAT3(1.f, -1.f, 0.f), DirectX::XMFLOAT2(1.f, 1.f)},
	{DirectX::XMFLOAT3(-1.f, 1.f, 0.f), DirectX::XMFLOAT2(0.f, 0.f)},
	{DirectX::XMFLOAT3(1.f, 1.f, 0.f), DirectX::XMFLOAT2(1.f, 0.f)},
};

ID3D11UnorderedAccessView* const kNullUAV[1] = { nullptr };
ID3D11ShaderResourceView* const kNullSRV[1] = { nullptr };

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_BIASVIEWER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    HWND window;

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow, window))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_BIASVIEWER));

    // verify DLL
    assert(getInt() == 5);

    HRESULT hr = S_OK;

    ID3D11Device* device;
    ID3D11DeviceContext* context;
    IDXGIDevice* dxgiDevice;
    IDXGIAdapter* dxgiAdapter;
    IDXGIFactory2* dxgiFactory;
    IDXGIOutput* dxgiOutput;
    IDXGIOutput1* dxgiOutput1;
    IDXGIOutputDuplication* duplication;
    ID3D11Texture2D* desktopSurfaceTexture = nullptr;
    IDXGISwapChain1* swapchain;

    ID3D11RenderTargetView* renderTargetView = nullptr;
    ID3D11SamplerState* samplerState;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* inputLayout;
    ID3D11Buffer* vertexBuffer;

    ID3D11ComputeShader* computeShader = nullptr;
    ID3D11Buffer* downsampleConstantBUffer = nullptr;

    std::vector<ID3D11Texture2D*> downsampleTextures;
    ID3D11Texture2D* ledTexture;

    std::vector<hvk::Color> ledColors;
    ledColors.resize(5 * 8);

    D3D_FEATURE_LEVEL FeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0
    };

    D3D_FEATURE_LEVEL availableFeatureLevel;

    hr = D3D11CreateDevice(
        nullptr, 
        D3D_DRIVER_TYPE_HARDWARE, 
        nullptr, 
        D3D11_CREATE_DEVICE_DEBUG,
        FeatureLevels, 
        1, 
        D3D11_SDK_VERSION, 
        &device, 
        &availableFeatureLevel, 
        &context);

    assert(SUCCEEDED(hr));

    hr = device->QueryInterface<IDXGIDevice>(&dxgiDevice);
    assert(SUCCEEDED(hr));

    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    assert(SUCCEEDED(hr));

    dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory));

    CaptureManager cm(device, dxgiAdapter);

    // initialize Arduino Control
    hvk::control::ArduinoController arduinoController;
    arduinoController.Init();

    HWND desktopWindow = GetDesktopWindow();
    RECT desktopRect;
    GetWindowRect(desktopWindow, &desktopRect);
    UINT desktopWidth = desktopRect.right - desktopRect.left;
    UINT desktopHeight = desktopRect.bottom - desktopRect.top;

    // prepare swapchain
    RECT windowRect;
    GetClientRect(window, &windowRect);
    UINT windowWidth = windowRect.right - windowRect.left;
    UINT windowHeight = windowRect.bottom - windowRect.top;

    // calculate number of downsample textures to use
    //uint8_t numFilters = ceil(max(std::log(desktopWidth) / std::log(8), std::log(desktopHeight) / std::log(8)));
    uint8_t numFilters = 0;
    UINT dividedWidth = desktopWidth;
    UINT dividedHeight = desktopHeight;
    for (uint8_t i = 1; (dividedWidth /= 8) >= 32; ++i)
    {
        numFilters = i;
    }
    // create with the number of downsample filters plus one for the full resolution
    //downsampleTextures.resize(numFilters + 1);
    downsampleTextures.resize(1);

    // prepare UAVs for filtering
    dividedWidth = desktopWidth;
	D3D11_TEXTURE2D_DESC texDesc;
    for (auto& filterTexture : downsampleTextures)
    {
		texDesc.Width = dividedWidth;
		texDesc.Height = dividedHeight;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.CPUAccessFlags = 0;
		//texDesc.MipLevels = 1;
		//texDesc.MipLevels = numFilters + 1;
		texDesc.MipLevels = 6;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		//texDesc.BindFlags = D3D11IND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		//texDesc.MiscFlags = 0;
		texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
		hr = device->CreateTexture2D(&texDesc, nullptr, &filterTexture);
        assert(hr == S_OK);

        dividedWidth /= 8;
        dividedHeight /= 8;

        // ensure power-of-two values to make LED processing easier
        dividedWidth = std::pow(2, std::ceil(std::log(dividedWidth) / std::log(2)));
        dividedHeight = std::pow(2, std::ceil(std::log(dividedHeight) / std::log(2)));
    }

    // create staging texture for LED processing
    texDesc.Width = 107;
    texDesc.Height = 45;
	texDesc.Usage = D3D11_USAGE_STAGING;
	texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	texDesc.BindFlags = 0;
    texDesc.MiscFlags = 0;
    texDesc.MipLevels = 1;
	hr = device->CreateTexture2D(&texDesc, nullptr, &ledTexture);
    assert(hr == S_OK);


    DXGI_SWAP_CHAIN_DESC1 swapchainDesc;
    RtlZeroMemory(&swapchainDesc, sizeof(swapchainDesc));
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapchainDesc.BufferCount = 2;
    swapchainDesc.Width = windowWidth;
    swapchainDesc.Height = windowHeight;
    swapchainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    hr = dxgiFactory->CreateSwapChainForHwnd(device, window, &swapchainDesc, nullptr, nullptr, &swapchain);
    if (FAILED(hr))
    {
        return hr;
    }


    // prepare shaders
    DWORD vertexBytesRead;
    std::vector<char> vertexByteCode;
    vertexByteCode.resize(20000);
    HANDLE vertexByteCodeHandle = CreateFile2(L"shaders\\compiled\\vertex.cso", GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
    bool readSuccess = ReadFile(vertexByteCodeHandle, vertexByteCode.data(), 20000, &vertexBytesRead, nullptr);
    hr = device->CreateVertexShader(vertexByteCode.data(), vertexBytesRead, nullptr, &vertexShader);

    DWORD pixelBytesRead;
    std::vector<char> pixelByteCode;
    pixelByteCode.resize(20000);
    HANDLE pixelByteCodeHandle = CreateFile2(L"shaders\\compiled\\pixel.cso", GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
    readSuccess = ReadFile(pixelByteCodeHandle, pixelByteCode.data(), 20000, &pixelBytesRead, nullptr);
    hr = device->CreatePixelShader(pixelByteCode.data(), pixelBytesRead, nullptr, &pixelShader);

    DWORD computeBytesRead;
    std::vector<char> computeByteCode;
    computeByteCode.resize(30000);
    HANDLE computeByteCodeHandle = CreateFile2(L"shaders\\compiled\\compute_filter.cso", GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
    readSuccess = ReadFile(computeByteCodeHandle, computeByteCode.data(), 30000, &computeBytesRead, nullptr);
    hr = device->CreateComputeShader(computeByteCode.data(), computeBytesRead, nullptr, &computeShader);

    D3D11_INPUT_ELEMENT_DESC vertexLayout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = device->CreateInputLayout(vertexLayout, 2, vertexByteCode.data(), vertexBytesRead, &inputLayout);
    context->IASetInputLayout(inputLayout);

    // prepare sampler
    D3D11_SAMPLER_DESC sampleDesc;
    RtlZeroMemory(&sampleDesc, sizeof(sampleDesc));
    sampleDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampleDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampleDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampleDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampleDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampleDesc.MinLOD = 0;
    sampleDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device->CreateSamplerState(&sampleDesc, &samplerState);
    if (FAILED(hr))
    {
        return hr;
    }

    // pre-prepare rendering pipeline
    FLOAT blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    //context->OMSetRenderTargets(1, &renderTargetView, nullptr);
    context->VSSetShader(vertexShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);
    //context->PSSetShaderResources(0, 1,)  // bind shader resource after obtaining surface capture
    context->PSSetSamplers(0, 1, &samplerState);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_BUFFER_DESC vertexBufferDesc;
    RtlZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(Vertex) * kNumVertices;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA vertexInitData;
    RtlZeroMemory(&vertexInitData, sizeof(vertexInitData));
    vertexInitData.pSysMem = kVertices;

    hr = device->CreateBuffer(&vertexBufferDesc, &vertexInitData, &vertexBuffer);

    UINT vertexStride = sizeof(Vertex);
    UINT vertexOffset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);

    // set viewport
    D3D11_VIEWPORT viewport;
    viewport.Width = static_cast<FLOAT>(windowWidth);
    viewport.Height = static_cast<FLOAT>(windowHeight);
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    context->RSSetViewports(1, &viewport);

	IDXGIResource* displayResource;
	DXGI_OUTDUPL_FRAME_INFO frameInfo;
    MSG msg;

    // Main message loop:
    while (true)
    {
        if (PeekMessage(&msg, window, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                break;
            }
			if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
        }

        if (desktopSurfaceTexture != nullptr)
        {
            desktopSurfaceTexture->Release();
        }
        hr = cm.AcquireFrameAsTexture(&desktopSurfaceTexture);
        assert(SUCCEEDED(hr));

        D3D11_TEXTURE2D_DESC surfaceDesc;
        desktopSurfaceTexture->GetDesc(&surfaceDesc);

		// prepare framebuffer target
		ID3D11Texture2D* backbuffer = nullptr;
		hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backbuffer));
		if (FAILED(hr))
		{
			return hr;
		}
        if (renderTargetView != nullptr)
        {
            renderTargetView->Release();
        }
		hr = device->CreateRenderTargetView(backbuffer, nullptr, &renderTargetView);
		backbuffer->Release();
		if (FAILED(hr))
		{
			return hr;
		}
		context->OMSetRenderTargets(1, &renderTargetView, nullptr);

        // copy the desktop surface to the texture we'll use for filtering
        //context->CopyResource(downsampleTextures[0], desktopSurfaceTexture);
        context->CopySubresourceRegion(downsampleTextures[0], 0, 0, 0, 0, desktopSurfaceTexture, 0, nullptr);
        D3D11_TEXTURE2D_DESC mipDesc;
        downsampleTextures[0]->GetDesc(&mipDesc);
		ID3D11ShaderResourceView* mipResourceView;
		D3D11_SHADER_RESOURCE_VIEW_DESC mipViewDesc;
		mipViewDesc.Format = mipDesc.Format;
		mipViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		mipViewDesc.Texture2D.MostDetailedMip = 0;
		mipViewDesc.Texture2D.MipLevels = mipDesc.MipLevels;
		hr = device->CreateShaderResourceView(downsampleTextures[0], &mipViewDesc, &mipResourceView);
        context->GenerateMips(mipResourceView);

		context->CSSetShader(computeShader, nullptr, 0);

        //UINT numDispatchX = 54;
        //UINT numDispatchY = 23;
        UINT numDispatchX = 0;
        UINT numDispatchY = 0;
   //     for (size_t i = 0; i < downsampleTextures.size()-1; ++i)
   //     {
   //         auto filterTexture = downsampleTextures[i];
   //         auto filterTo = downsampleTextures[i + 1];

   //         D3D11_TEXTURE2D_DESC filterDesc;
   //         filterTexture->GetDesc(&filterDesc);

			//D3D11_TEXTURE2D_DESC filterToDesc;
   //         filterTo->GetDesc(&filterToDesc);

   //         numDispatchX = filterDesc.Width / 64;
   //         numDispatchY = filterDesc.Height / 64;

			//D3D11_UNORDERED_ACCESS_VIEW_DESC filterUnorderedDesc;
			//filterUnorderedDesc.Format = filterToDesc.Format;
			//filterUnorderedDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			//filterUnorderedDesc.Texture2D.MipSlice = 0;

			//ID3D11UnorderedAccessView* filterView;
			//hr = device->CreateUnorderedAccessView(filterTo, &filterUnorderedDesc, &filterView);

			//D3D11_SHADER_RESOURCE_VIEW_DESC filterShaderDesc;
			//filterShaderDesc.Format = filterDesc.Format;
			//filterShaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			//filterShaderDesc.Texture2D.MostDetailedMip = filterDesc.MipLevels - 1;
			//filterShaderDesc.Texture2D.MipLevels = filterDesc.MipLevels;

			//ID3D11ShaderResourceView* filterShaderResource;
   //         hr = device->CreateShaderResourceView(filterTexture, &filterShaderDesc, &filterShaderResource);

   //         // create dispatch constant
   //         //DownsampleConstantBuffer db = { numDispatchX, numDispatchY };
   //         DownsampleConstantBuffer db = { filterDesc.Width / filterToDesc.Width, filterDesc.Height / filterToDesc.Height };
   //         D3D11_BUFFER_DESC dbDesc;
   //         dbDesc.ByteWidth = static_cast<UINT>(std::max(sizeof(DownsampleConstantBuffer), (size_t)16));
   //         dbDesc.Usage = D3D11_USAGE_DYNAMIC;
   //         dbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
   //         dbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
   //         dbDesc.MiscFlags = 0;
   //         dbDesc.StructureByteStride = 0;

   //         D3D11_SUBRESOURCE_DATA dbData;
   //         dbData.pSysMem = &db;
   //         dbData.SysMemPitch = 0;
   //         dbData.SysMemSlicePitch = 0;

   //         hr = device->CreateBuffer(&dbDesc, &dbData, &downsampleConstantBUffer);
   //         context->CSSetConstantBuffers(0, 1, &downsampleConstantBUffer);

			//// filter desktop image through compute shader
			//context->CSSetUnorderedAccessViews(0, 1, &filterView, nullptr);
   //         context->CSSetShaderResources(0, 1, &filterShaderResource);
			////context->Dispatch(430, 180, 1);
			//context->Dispatch(numDispatchX, numDispatchY, 1);
			//context->CSSetUnorderedAccessViews(0, 1, kNullUAV, nullptr);
   //         context->CSSetShaderResources(0, 1, kNullSRV);

   //         //numDispatchX /= 2;
   //         //numDispatchY /= 2;

   //         filterShaderResource->Release();
   //         filterView->Release();
   //         downsampleConstantBUffer->Release();

   //     }
		// clear compute shader context
		context->CSSetShader(nullptr, nullptr, 0);

        // copy the lowest resolution downsample texture to the LED staging texture
        context->CopySubresourceRegion(ledTexture, 0, 0, 0, 0, downsampleTextures[downsampleTextures.size() - 1], 5, nullptr);

        D3D11_TEXTURE2D_DESC filterDesc;
        downsampleTextures[downsampleTextures.size() - 1]->GetDesc(&filterDesc);
        //downsampleTextures[1]->GetDesc(&filterDesc);
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc;
        //shaderDesc.Format = surfaceDesc.Format;
        shaderDesc.Format = filterDesc.Format;
        shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        //shaderDesc.Texture2D.MostDetailedMip = filterDesc.MipLevels - 1;
        shaderDesc.Texture2D.MostDetailedMip = 0;
        shaderDesc.Texture2D.MipLevels = filterDesc.MipLevels;

        ID3D11ShaderResourceView* shaderResource;
        //hr = device->CreateShaderResourceView(desktopSurfaceTexture, &shaderDesc, &shaderResource);
        hr = device->CreateShaderResourceView(downsampleTextures[downsampleTextures.size()-1], &shaderDesc, &shaderResource);
        //hr = device->CreateShaderResourceView(downsampleTextures[1], &shaderDesc, &shaderResource);

		//context->PSSetShaderResources(0, 1,)  // bind shader resource after obtaining surface capture
        context->PSSetShaderResources(0, 1, &shaderResource);


        context->Draw(kNumVertices, 0);

        context->PSSetShaderResources(0, 1, kNullSRV);

        //duplication->ReleaseFrame();

        hr = swapchain->Present(1, 0);
        shaderResource->Release();

        // map the LED staging texture for CPU read
        D3D11_MAPPED_SUBRESOURCE mappedLEDData;
        ZeroMemory(&mappedLEDData, sizeof(D3D11_MAPPED_SUBRESOURCE));
        context->Map(ledTexture, 0, D3D11_MAP_READ, 0, &mappedLEDData);

        // calculate LED colors

        const uint32_t numComponents = 4;   // pixels in BGRA
        const uint32_t pixelsPerRow = 107;
        const uint32_t pixelsPerCol = 45;
        const uint32_t numXBuckets = 8;
        const uint32_t numYBuckets = 5;
        const uint32_t bucketWidth = pixelsPerRow / numXBuckets + 1;
        const uint32_t xRemainder = numXBuckets * bucketWidth - pixelsPerRow;
        const uint32_t bucketHeight = pixelsPerCol / numYBuckets + 1;
        const uint32_t yRemainder = numYBuckets * bucketHeight - pixelsPerCol;

        for (size_t i = 0; i < numYBuckets; ++i)
        {
            uint8_t destYOffset = i * numXBuckets;
            uint32_t yOffset = i * bucketHeight;
            if (i < yRemainder)
            {
                yOffset -= i;
            }
            for (size_t j = 0; j < numXBuckets; ++j)
            {
                uint8_t destXOffset = j;
                uint32_t xOffset = j * bucketWidth;
                if (j < xRemainder)
                {
                    xOffset -= j;
                }

                // now traverse the neighborhood of this bucket
                uint32_t aR = 0;
                uint32_t aG = 0;
                uint32_t aB = 0;
                for (size_t nY = 0; nY < bucketHeight; ++nY)
                {
                    uint32_t pixelY = yOffset + nY;
                    uint32_t scanOffset = pixelY * pixelsPerRow;
                    for (size_t nX = 0; nX < bucketWidth; ++nX)
                    {
                        uint32_t pixelX = xOffset + nX;
                        uint32_t pixelOffset = (scanOffset + pixelX) * numComponents;
                        aB += *(reinterpret_cast<uint8_t*>(mappedLEDData.pData) + pixelOffset);
                        aG += *(reinterpret_cast<uint8_t*>(mappedLEDData.pData) + pixelOffset + 1);
                        aR += *(reinterpret_cast<uint8_t*>(mappedLEDData.pData) + pixelOffset + 2);
                    }
                }
                aR = std::max((uint8_t)(aR / (bucketHeight * bucketWidth)), (uint8_t)1);
                aG = std::max((uint8_t)(aG / (bucketHeight * bucketWidth)), (uint8_t)1);
                aB = std::max((uint8_t)(aB / (bucketHeight * bucketWidth)), (uint8_t)1);

                ledColors[destYOffset + destXOffset] = { (uint8_t)aR, (uint8_t)aG, (uint8_t)aB };
            }
        }

  //      const uint32_t numPixelsPerBucket = 

  //      const uint8_t consumeX = std::ceil(53 / 8);
  //      const uint8_t consumeY = std::ceil(22 / 8);
		//uint8_t* dataAddress = reinterpret_cast<uint8_t*>(mappedLEDData.pData);
  //      for (size_t i = 0; i < 5; ++i)
  //      {
  //          uint8_t yStart = i * 22 / 5;
  //          for (size_t j = 0; j < 8; ++j)
  //          {
  //              //uint8_t xStart = j * 53 / 8;
  //              //auto& thisLEDColor = ledColors[i * 8 + j];
  //              //size_t lookup = (yStart * 53) + (xStart * 22) * 4;
  //              //uint8_t* offset = reinterpret_cast<uint8_t*>(mappedLEDData.pData) + lookup;
  //              //thisLEDColor.b = std::max(*offset, (uint8_t)1);
  //              //thisLEDColor.g = std::max(*(offset + 1), (uint8_t)1);
  //              //thisLEDColor.r = std::max(*(offset + 2), (uint8_t)1);


  //              uint32_t newRed = 0;
  //              uint32_t newGreen = 0;
  //              uint32_t newBlue = 0;
  //              uint32_t baseOffset = i * 8 * (consumeX * consumeY) * 4;
  //              for (uint8_t k = 0; k < consumeY; ++k)
  //              {
  //                  for (uint8_t l = 0; l < consumeX; ++l)
  //                  {
  //                      uint32_t offset = (baseOffset + (k * consumeX) + l) * 4;
  //                      newBlue += *(dataAddress + offset);
  //                      newGreen += *(dataAddress + offset + 1);
  //                      newRed += *(dataAddress + offset + 2);
  //                  }
  //              }
  //              newRed /= (consumeX * consumeY);
  //              newGreen /= (consumeX * consumeY);
  //              newBlue /= (consumeX * consumeY);

  //              //ledColors[i * 8 + j] = 
  //              //{ 
  //              //    std::max((uint8_t)newRed, (uint8_t)1), 
  //              //    std::max((uint8_t)newGreen, (uint8_t)1), 
  //              //    std::max((uint8_t)newBlue, (uint8_t)1) 
  //              //};
  //              ledColors[i * 8 + j] = { 255, 1, 1 };
  //          }
  //      }

  //      arduinoController.WritePixels(ledColors);

        arduinoController.WritePixels(ledColors);
        context->Unmap(ledTexture, 0);
		Sleep(32);
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BIASVIEWER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_BIASVIEWER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, HWND& window)
{
   hInst = hInstance; // Store instance handle in our global variable

   window = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!window)
   {
      return FALSE;
   }

   ShowWindow(window, nCmdShow);
   UpdateWindow(window);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
