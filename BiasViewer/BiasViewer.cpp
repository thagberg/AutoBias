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
    ID3D11Texture2D* desktopSurfaceTexture;
    IDXGISwapChain1* swapchain;

    ID3D11RenderTargetView* renderTargetView;
    ID3D11SamplerState* samplerState;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* inputLayout;
    ID3D11Buffer* vertexBuffer;

    ID3D11ComputeShader* computeShader = nullptr;
    ID3D11Buffer* downsampleConstantBUffer = nullptr;

    std::vector<ID3D11Texture2D*> downsampleTextures;

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

    if (!SUCCEEDED(hr))
    {
        std::cerr << "Failed to create D3D11 Device and Context" << std::endl;
        return hr;
    }

    CaptureManager cm(device);

    //hr = device->QueryInterface<IDXGIDevice>(&dxgiDevice);

    //if (!SUCCEEDED(hr))
    //{
    //    std::cerr << "Failed to create DXGI Device" << std::endl;
    //    return hr;
    //}

    ////hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&DxgiAdapter));
    //hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter));

    //if (!SUCCEEDED(hr))
    //{
    //    std::cerr << "Failed to obtain DXGI Adapter" << std::endl;
    //    return hr;
    //}

    //hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory));
    //if (!SUCCEEDED(hr))
    //{
    //    return hr;
    //}

    //hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);

    //if (!SUCCEEDED(hr))
    //{
    //    std::cerr << "Failed to obtain DXGI Output" << std::endl;
    //    return hr;
    //}

    //hr = dxgiOutput->QueryInterface<IDXGIOutput1>(&dxgiOutput1);

    //if (!SUCCEEDED(hr))
    //{
    //    std::cerr << "Failed to obtain DXGI Output 1" << std::endl;
    //    return hr;
    //}

    //hr = dxgiOutput1->DuplicateOutput(device, &duplication);

    //if (!SUCCEEDED(hr))
    //{
    //    std::cerr << "Failed to duplicate output" << std::endl;
    //    return hr;
    //}

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
    for (uint8_t i = 1; (dividedWidth /= 8) >= 21; ++i)
    {
        numFilters = i;
    }
    // create with the number of downsample filters plus one for the full resolution
    downsampleTextures.resize(numFilters + 1);

    // prepare UAVs for filtering
    dividedWidth = desktopWidth;
    for (auto& filterTexture : downsampleTextures)
    {
		D3D11_TEXTURE2D_DESC texDesc;
		texDesc.Width = dividedWidth;
		texDesc.Height = dividedHeight;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.CPUAccessFlags = 0;
		texDesc.MipLevels = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		texDesc.MiscFlags = 0;
		hr = device->CreateTexture2D(&texDesc, nullptr, &filterTexture);

        dividedWidth /= 8;
        dividedHeight /= 8;
    }


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

		hr = duplication->AcquireNextFrame(INFINITE, &frameInfo, &displayResource);
		if (!SUCCEEDED(hr))
		{
            std::cout << "Failed to acquire frame" << std::endl;
		}

		hr = displayResource->QueryInterface<ID3D11Texture2D>(&desktopSurfaceTexture);
		if (!SUCCEEDED(hr))
		{
            std::cout << "Failed to get texture from frame resource" << std::endl;
		}


        D3D11_TEXTURE2D_DESC surfaceDesc;
        desktopSurfaceTexture->GetDesc(&surfaceDesc);

		// prepare framebuffer target
		ID3D11Texture2D* backbuffer = nullptr;
		hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backbuffer));
		if (FAILED(hr))
		{
			return hr;
		}
		hr = device->CreateRenderTargetView(backbuffer, nullptr, &renderTargetView);
		backbuffer->Release();
		if (FAILED(hr))
		{
			return hr;
		}
		context->OMSetRenderTargets(1, &renderTargetView, nullptr);

        // copy the desktop surface to the texture we'll use for filtering
        context->CopyResource(downsampleTextures[0], desktopSurfaceTexture);

		context->CSSetShader(computeShader, nullptr, 0);

        UINT numDispatchX = 54;
        UINT numDispatchY = 23;
        for (size_t i = 0; i < downsampleTextures.size()-1; ++i)
        {
            auto filterTexture = downsampleTextures[i];
            auto filterTo = downsampleTextures[i + 1];

			D3D11_TEXTURE2D_DESC filterToDesc;
			filterTo->GetDesc(&filterToDesc);

			D3D11_UNORDERED_ACCESS_VIEW_DESC filterUnorderedDesc;
			filterUnorderedDesc.Format = filterToDesc.Format;
			filterUnorderedDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			filterUnorderedDesc.Texture2D.MipSlice = 0;

			ID3D11UnorderedAccessView* filterView;
			hr = device->CreateUnorderedAccessView(filterTo, &filterUnorderedDesc, &filterView);

            D3D11_TEXTURE2D_DESC filterDesc;
            filterTexture->GetDesc(&filterDesc);
			D3D11_SHADER_RESOURCE_VIEW_DESC filterShaderDesc;
			filterShaderDesc.Format = filterDesc.Format;
			filterShaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			filterShaderDesc.Texture2D.MostDetailedMip = filterDesc.MipLevels - 1;
			filterShaderDesc.Texture2D.MipLevels = filterDesc.MipLevels;

			ID3D11ShaderResourceView* filterShaderResource;
            hr = device->CreateShaderResourceView(filterTexture, &filterShaderDesc, &filterShaderResource);

            // create dispatch constant
            DownsampleConstantBuffer db = { numDispatchX, numDispatchY };
            D3D11_BUFFER_DESC dbDesc;
            dbDesc.ByteWidth = static_cast<UINT>(std::max(sizeof(DownsampleConstantBuffer), (size_t)16));
            dbDesc.Usage = D3D11_USAGE_DYNAMIC;
            dbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            dbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            dbDesc.MiscFlags = 0;
            dbDesc.StructureByteStride = 0;

            D3D11_SUBRESOURCE_DATA dbData;
            dbData.pSysMem = &db;
            dbData.SysMemPitch = 0;
            dbData.SysMemSlicePitch = 0;

            hr = device->CreateBuffer(&dbDesc, &dbData, &downsampleConstantBUffer);
            context->CSSetConstantBuffers(0, 1, &downsampleConstantBUffer);

			// filter desktop image through compute shader
			context->CSSetUnorderedAccessViews(0, 1, &filterView, nullptr);
            context->CSSetShaderResources(0, 1, &filterShaderResource);
			//context->Dispatch(430, 180, 1);
			context->Dispatch(numDispatchX, numDispatchY, 1);
			context->CSSetUnorderedAccessViews(0, 1, kNullUAV, nullptr);
            context->CSSetShaderResources(0, 1, kNullSRV);

            numDispatchX /= 2;
            numDispatchY /= 2;

        }
		// clear compute shader context
		context->CSSetShader(nullptr, nullptr, 0);


        D3D11_TEXTURE2D_DESC filterDesc;
        downsampleTextures[downsampleTextures.size() - 1]->GetDesc(&filterDesc);
        //downsampleTextures[1]->GetDesc(&filterDesc);
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc;
        //shaderDesc.Format = surfaceDesc.Format;
        shaderDesc.Format = filterDesc.Format;
        shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderDesc.Texture2D.MostDetailedMip = filterDesc.MipLevels - 1;
        shaderDesc.Texture2D.MipLevels = filterDesc.MipLevels;

        ID3D11ShaderResourceView* shaderResource;
        //hr = device->CreateShaderResourceView(desktopSurfaceTexture, &shaderDesc, &shaderResource);
        hr = device->CreateShaderResourceView(downsampleTextures[downsampleTextures.size()-1], &shaderDesc, &shaderResource);
        //hr = device->CreateShaderResourceView(downsampleTextures[1], &shaderDesc, &shaderResource);

		//context->PSSetShaderResources(0, 1,)  // bind shader resource after obtaining surface capture
        context->PSSetShaderResources(0, 1, &shaderResource);


        context->Draw(kNumVertices, 0);

        context->PSSetShaderResources(0, 1, kNullSRV);

        duplication->ReleaseFrame();

        hr = swapchain->Present(1, 0);
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
