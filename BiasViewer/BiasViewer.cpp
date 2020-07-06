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
#include <array>

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

const uint8_t kGridWidth = 8;
const uint8_t kGridHeight = 5;

struct Vertex
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 TexCoord;
};

const uint8_t kNumVertices = 6;
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

    HRESULT hr = S_OK;

    ID3D11Device* device;
    ID3D11DeviceContext* context;
    IDXGIDevice* dxgiDevice;
    IDXGIAdapter* dxgiAdapter;
    IDXGIFactory2* dxgiFactory;
    ID3D11Texture2D* desktopSurfaceTexture = nullptr;
    IDXGISwapChain1* swapchain;

    ID3D11RenderTargetView* renderTargetView = nullptr;
    ID3D11SamplerState* samplerState;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* inputLayout;
    ID3D11Buffer* vertexBuffer;

    ID3D11Texture2D* downsampleTexture;
    ID3D11Texture2D* ledTexture;
    ID3D11Texture2D* ledToRenderTexture;

    std::array<hvk::Color, 8*5> ledColors;

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

    hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory));
    assert(SUCCEEDED(hr));

    CaptureManager cm;
    hr = cm.Init(device, dxgiAdapter);
    assert(hr == S_OK);

    // initialize Arduino Control
    hvk::control::ArduinoController<kGridWidth, kGridHeight> arduinoController;
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

    // Calculate number of mipmaps to generate on desktop texture
    // we want one pixel "bucket" or "neighborhood" per LED
    // buckets need to be at least 10x10 pixels for proper
    // color importance sampling (TODO)
    uint32_t numMips = 0;
    {
        const float bucketDimension = 10.f;
        const uint32_t maxX = desktopWidth / kGridWidth;
        const uint32_t maxY = desktopHeight / kGridHeight;
        // "This is stupid, why do I need to pay attention in Algebra 2" -- Me, 14 years old
        const uint32_t xMips = std::floor(log2(desktopWidth / (bucketDimension * log2(maxX))));
        const uint32_t yMips = std::floor(log2(desktopHeight / (bucketDimension * log2(maxY))));
		numMips = std::min(xMips, yMips);
    }
    assert(numMips > 1);

    // prepare UAV texture
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = desktopWidth;
	texDesc.Height = desktopHeight;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.CPUAccessFlags = 0;
	texDesc.MipLevels = numMips;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	hr = device->CreateTexture2D(&texDesc, nullptr, &downsampleTexture);
	assert(hr == S_OK);

    // create staging texture for LED processing
    // dimensions based on lowest desktop mipmap
    texDesc.Width = desktopWidth >> (numMips-1);
    texDesc.Height = desktopHeight >> (numMips-1);
	texDesc.Usage = D3D11_USAGE_STAGING;
	texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	texDesc.BindFlags = 0;
    texDesc.MiscFlags = 0;
    texDesc.MipLevels = 1;
	hr = device->CreateTexture2D(&texDesc, nullptr, &ledTexture);
    assert(hr == S_OK);

    // create staging texture for copying LED data back to GPU 
    texDesc.Width = kGridWidth;
    texDesc.Height = kGridHeight;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.CPUAccessFlags = 0;
	texDesc.BindFlags = 0;
    texDesc.MiscFlags = 0;
    texDesc.MipLevels = 1;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	hr = device->CreateTexture2D(&texDesc, nullptr, &ledToRenderTexture);
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
    assert(SUCCEEDED(hr));


    // prepare shaders
	std::vector<char> vertexByteCode;
	std::vector<char> pixelByteCode;
	DWORD vertexBytesRead;
	DWORD pixelBytesRead;
    {
        vertexByteCode.resize(20000);
        HANDLE vertexByteCodeHandle = CreateFile2(L"shaders\\vertex.cso", GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
        bool readSuccess = ReadFile(vertexByteCodeHandle, vertexByteCode.data(), 20000, &vertexBytesRead, nullptr);
        hr = device->CreateVertexShader(vertexByteCode.data(), vertexBytesRead, nullptr, &vertexShader);
        assert(hr == S_OK);
        bool closeSuccess = CloseHandle(vertexByteCodeHandle);
        assert(closeSuccess);

        pixelByteCode.resize(20000);
        HANDLE pixelByteCodeHandle = CreateFile2(L"shaders\\pixel.cso", GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
        readSuccess = ReadFile(pixelByteCodeHandle, pixelByteCode.data(), 20000, &pixelBytesRead, nullptr);
        hr = device->CreatePixelShader(pixelByteCode.data(), pixelBytesRead, nullptr, &pixelShader);
        assert(hr == S_OK);
        closeSuccess = CloseHandle(pixelByteCodeHandle);
        assert(closeSuccess);
    }

    D3D11_INPUT_ELEMENT_DESC vertexLayout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = device->CreateInputLayout(vertexLayout, 2, vertexByteCode.data(), vertexBytesRead, &inputLayout);
    assert(hr == S_OK);
    context->IASetInputLayout(inputLayout);

    // can now release shader bytecode buffers
    vertexByteCode.resize(0);
    pixelByteCode.resize(0);

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
    assert(SUCCEEDED(hr));

    // pre-prepare rendering pipeline
    FLOAT blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    context->VSSetShader(vertexShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);
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

    // Main loop
    MSG msg;
    bool running = true;
    while (running)
    {
        // process window messages
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT || msg.message == WM_CLOSE || msg.message == WM_DESTROY)
            {
                running = false;
            }
			if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
        }

        // release previous frame's surface before acquiring the next
        if (desktopSurfaceTexture != nullptr)
        {
            desktopSurfaceTexture->Release();
        }
        hr = cm.AcquireFrameAsTexture(&desktopSurfaceTexture);
        // if we failed to acquire a frame, the desktop resolution or format could be changing, retry
        if (!SUCCEEDED(hr))
        {
            uint8_t numRetries = 0;
            while (numRetries < 10)
            {
                Sleep(100);
				cm = CaptureManager();
				hr = cm.Init(device, dxgiAdapter);
                if (hr != S_OK)
                {
                    ++numRetries;
                }
                else
                {
					hr = cm.AcquireFrameAsTexture(&desktopSurfaceTexture);
					if (!SUCCEEDED(hr))
					{
						++numRetries;
					}
					else
					{
						break;
					}
                }
            }
			assert(SUCCEEDED(hr));
            continue;
        }

        // copy the desktop surface to the texture we'll use for filtering
        context->CopySubresourceRegion(downsampleTexture, 0, 0, 0, 0, desktopSurfaceTexture, 0, nullptr);

        // surface has been copied, so we can now release the frame
        desktopSurfaceTexture->Release();
        cm.ReleaseFrame();

        // create a resource view for the texture so that we can generate mip-maps
        D3D11_TEXTURE2D_DESC mipDesc;
        downsampleTexture->GetDesc(&mipDesc);
		ID3D11ShaderResourceView* mipResourceView;
		D3D11_SHADER_RESOURCE_VIEW_DESC mipViewDesc;
		mipViewDesc.Format = mipDesc.Format;
		mipViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		mipViewDesc.Texture2D.MostDetailedMip = 0;
		mipViewDesc.Texture2D.MipLevels = mipDesc.MipLevels;
		hr = device->CreateShaderResourceView(downsampleTexture, &mipViewDesc, &mipResourceView);
        context->GenerateMips(mipResourceView);

        // copy a low-resolution mipmap to the LED staging texture
        context->CopySubresourceRegion(ledTexture, 0, 0, 0, 0, downsampleTexture, numMips-1, nullptr);
        D3D11_TEXTURE2D_DESC ledDesc;
        ledTexture->GetDesc(&ledDesc);

        const uint32_t pixelsPerRow = ledDesc.Width;
        const uint32_t pixelsPerCol = ledDesc.Height;
        const uint32_t numXBuckets = kGridWidth;
        const uint32_t numYBuckets = kGridHeight;
        const uint32_t bucketWidth = std::ceil(static_cast<float>(pixelsPerRow) / numXBuckets);
        const uint32_t xRemainder = numXBuckets * bucketWidth - pixelsPerRow;
        const uint32_t bucketHeight = std::ceil(static_cast<float>(pixelsPerCol) / numYBuckets);
        const uint32_t yRemainder = numYBuckets * bucketHeight - pixelsPerCol;

        // create a texture for copying buckets into and mapping for CPU read
		D3D11_MAPPED_SUBRESOURCE mappedBucketData;
        ID3D11Texture2D* bucketTex;
		D3D11_TEXTURE2D_DESC bucketDesc;
		bucketDesc.Width = bucketWidth;
		bucketDesc.Height = bucketHeight;
        bucketDesc.ArraySize = 1;
        bucketDesc.Format = ledDesc.Format;
		bucketDesc.Usage = D3D11_USAGE_STAGING;
		bucketDesc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
		bucketDesc.MiscFlags = 0;
		bucketDesc.MipLevels = 1;
        bucketDesc.BindFlags = 0;
		bucketDesc.SampleDesc.Count = 1;
		bucketDesc.SampleDesc.Quality = 0;
        hr = device->CreateTexture2D(&bucketDesc, nullptr, &bucketTex);
        assert(hr == S_OK);

        // this is probably a very inefficient way of doing this, but it's hard to get wrong this way
        // copy each "bucket" into a new texture as a subresource region, then map it to CPU memory
        // and traverse it linearly to calculate the neighborhood color
        uint32_t yOffset = 0;
        uint32_t xOffset = 0;
        for (uint8_t i = 0; i < kGridHeight; ++i)
        {
            for (uint8_t j = 0; j < kGridWidth; ++j)
            {
                D3D11_BOX copyRegion;
                copyRegion.top = yOffset;
                copyRegion.left = xOffset;
                copyRegion.right = xOffset + bucketWidth;
                copyRegion.bottom = yOffset + bucketHeight;
                copyRegion.front = 0;
                copyRegion.back = 1;

                context->CopySubresourceRegion(bucketTex, 0, 0, 0, 0, ledTexture, 0, &copyRegion);

                ZeroMemory(&mappedBucketData, sizeof(D3D11_MAPPED_SUBRESOURCE));
                context->Map(bucketTex, 0, D3D11_MAP_READ, 0, &mappedBucketData);

                uint32_t aR = 0;
                uint32_t aG = 0;
                uint32_t aB = 0;
                uint32_t pixelOffset = 0;
                while (pixelOffset*4 < mappedBucketData.DepthPitch)
                {
                    aB += *(reinterpret_cast<uint8_t*>(mappedBucketData.pData) + pixelOffset);
                    aG += *(reinterpret_cast<uint8_t*>(mappedBucketData.pData) + pixelOffset + 1);
                    aR += *(reinterpret_cast<uint8_t*>(mappedBucketData.pData) + pixelOffset + 2);

                    pixelOffset += 4;
                }
                ledColors[i * kGridWidth + j] = { 
                    (uint8_t)(aR / (bucketWidth * bucketHeight)), 
                    (uint8_t)(aG / (bucketWidth * bucketHeight)),
                    (uint8_t)(aB / (bucketWidth * bucketHeight)) };

                context->Unmap(bucketTex, 0);
                xOffset += bucketWidth;
                if (j < xRemainder)
                {
                    xOffset--;
                }
            }
            xOffset = 0;
            yOffset += bucketHeight;
            if (i < yRemainder)
            {
                yOffset -= 1;
            }
        }

        // write colors out to microcontroller
        arduinoController.WritePixels(ledColors);

        // write data to texture for the viewer app
        std::vector<uint8_t> ledData;
        ledData.resize(ledColors.size() * 4);
        for (size_t i = 0; i < ledColors.size(); ++i)
        {
            ledData[i * 4] = ledColors[i].b;
            ledData[i * 4 + 1] = ledColors[i].g;
            ledData[i * 4 + 2] = ledColors[i].r;
            ledData[i * 4 + 3] = 255;
        }
        context->UpdateSubresource(ledToRenderTexture, 0, nullptr, ledData.data(), 4 * 8, 4 * 8 * 5);

		// prepare framebuffer for presentation
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

        // bind texture to shader
        D3D11_TEXTURE2D_DESC filterDesc;
        downsampleTexture->GetDesc(&filterDesc);
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc;
        shaderDesc.Format = filterDesc.Format;
        shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderDesc.Texture2D.MostDetailedMip = 0;
        shaderDesc.Texture2D.MipLevels = 1;
        ID3D11ShaderResourceView* shaderResource;
        hr = device->CreateShaderResourceView(ledToRenderTexture, &shaderDesc, &shaderResource);
        context->PSSetShaderResources(0, 1, &shaderResource);

        // draw the viewer
        context->Draw(kNumVertices, 0);

        // null out shader resources to prepare for next frame
        shaderResource->Release();
        context->PSSetShaderResources(0, 1, kNullSRV);


        hr = swapchain->Present(1, 0);
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
