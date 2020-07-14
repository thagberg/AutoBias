// D3D12Viewer.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "D3D12Viewer.h"

#include <d3d12.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>

#include <Render.h>
#include <CaptureManager.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#define MAX_LOADSTRING 100

const size_t kFramebuffers = 2;

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

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int, HWND& window);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

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
    LoadStringW(hInstance, IDC_D3D12VIEWER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    HWND window;

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow, window))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_D3D12VIEWER));

    //-------------------------------------------
    HWND desktopWindow = GetDesktopWindow();
    RECT desktopRect;
    GetWindowRect(desktopWindow, &desktopRect);
    uint32_t desktopWidth = desktopRect.right - desktopRect.left;
    uint32_t desktopHeight = desktopRect.bottom - desktopRect.top;

    RECT windowRect;
    GetClientRect(window, &windowRect);
    uint32_t windowWidth = windowRect.right - windowRect.left;
    uint32_t windowHeight = windowRect.bottom - windowRect.top;

    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter1> hardwareAdapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<IDXGISwapChain1> swapchain;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12DescriptorHeap> uavHeap;
    ComPtr<ID3D12DescriptorHeap> samplerHeap;

    HRESULT hr = S_OK;
    hr = hvk::render::CreateFactory(factory);
    hvk::render::GetHardwareAdapter(factory.Get(), &hardwareAdapter);
    hr = hvk::render::CreateDevice(factory, hardwareAdapter, device);
    hr = hvk::render::CreateCommandQueue(device, commandQueue);
    hr = hvk::render::CreateSwapchain(commandQueue, factory, window, 2, windowWidth, windowHeight, swapchain);
    hr = hvk::render::CreateDescriptorHeaps(device, 2, rtvHeap, uavHeap, samplerHeap);

    ComPtr<ID3D12Resource> vertexBuffer;
    auto vertexBufferView = hvk::render::CreateVertexBuffer(
        device, 
        reinterpret_cast<const uint8_t*>(kVertices), 
        sizeof(kVertices), 
        sizeof(Vertex), 
        vertexBuffer);

    std::vector<uint8_t> vertexByteCode;
    std::vector<uint8_t> pixelByteCode;
    DWORD codeSize;
    DWORD bytesRead;
    HANDLE vertexHandle = CreateFile2(L"shaders\\vertex.cso", GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
    codeSize = GetFileSize(vertexHandle, nullptr);
    vertexByteCode.resize(codeSize);
    bool readSuccess = ReadFile(vertexHandle, vertexByteCode.data(), codeSize, &bytesRead, nullptr);
    assert(readSuccess);
    assert(bytesRead == codeSize);
    bool closeSuccess = CloseHandle(vertexHandle);
    assert(closeSuccess);
    HANDLE pixelHandle = CreateFile2(L"shaders\\pixel.cso", GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
    codeSize = GetFileSize(pixelHandle, nullptr);
    pixelByteCode.resize(codeSize);
    readSuccess = ReadFile(pixelHandle, pixelByteCode.data(), codeSize, &bytesRead, nullptr);
    assert(readSuccess);
    assert(bytesRead == codeSize);
    closeSuccess = CloseHandle(pixelHandle);
    assert(closeSuccess);

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0.f;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    sampler.MinLOD = 0;
    sampler.MaxLOD = 0;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_DESCRIPTOR_RANGE texRange = {};
    texRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texRange.NumDescriptors = 1;
    texRange.BaseShaderRegister = 0;
    texRange.RegisterSpace = 0;
    texRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER texParam = {};
    texParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    texParam.DescriptorTable.NumDescriptorRanges = 1;
    texParam.DescriptorTable.pDescriptorRanges = &texRange;
    texParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    ComPtr<ID3D12RootSignature> rootSignature;
    hr = hvk::render::CreateRootSignature(device, { texParam }, { sampler }, rootSignature);
    assert(SUCCEEDED(hr));

    D3D12_INPUT_ELEMENT_DESC vertexInputs[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 3 * sizeof(float), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    D3D12_INPUT_LAYOUT_DESC vertexLayout = {};
    vertexLayout.NumElements = sizeof(vertexInputs) / sizeof(D3D12_INPUT_ELEMENT_DESC);
    vertexLayout.pInputElementDescs = vertexInputs;

    ComPtr<ID3D12PipelineState> pipelineState;
    hr = hvk::render::CreateGraphicsPipelineState(device, vertexLayout, rootSignature, vertexByteCode.data(), vertexByteCode.size(), pixelByteCode.data(), pixelByteCode.size(), pipelineState);
    assert(SUCCEEDED(hr));

    ComPtr<ID3D12CommandAllocator> commandAllocator;
    hr = hvk::render::CreateCommandAllocator(device, commandAllocator);
    assert(SUCCEEDED(hr));

    ComPtr<ID3D12GraphicsCommandList> commandList;
    hr = hvk::render::CreateCommandList(device, commandAllocator, pipelineState, commandList);
    assert(SUCCEEDED(hr));
    commandList->Close();

    ComPtr<ID3D12Resource> rendertargets[kFramebuffers];
    hr = hvk::render::CreateRenderTargetView(device, swapchain, rtvHeap, kFramebuffers, rendertargets);
    assert(SUCCEEDED(hr));

    D3D12_VIEWPORT viewport = {};
    viewport.Width = windowWidth;
    viewport.Height = windowHeight;
    viewport.TopLeftX = 0.f;
    viewport.TopLeftY = 0.f;
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;

    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.right = windowWidth;
    scissorRect.top = 0;
    scissorRect.bottom = windowHeight;

    CaptureManager cm;
    hr = cm.Init();

    MSG msg;
    bool running = true;

    //ID3D11Texture2D* desktopSurfaceTexture = nullptr;
    //IDXGISurface* desktopSurface = nullptr;
    IDXGIResource* desktopResource = nullptr;

    uint8_t frameIndex = 0;
    while (running)
    {
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
        if (desktopResource != nullptr)
        {
            desktopResource->Release();
        }
        hr = cm.AcquireFrameAsDXGIResource(&desktopResource);
        IDXGIResource1* sharedResource;
        desktopResource->QueryInterface<IDXGIResource1>(&sharedResource);
        HANDLE hTexture;
        sharedResource->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &hTexture);
        ID3D12Resource* d3d12Resource;
        device->OpenSharedHandle(hTexture, __uuidof(ID3D12Resource), (void**)&d3d12Resource);
        auto resourceDesc = d3d12Resource->GetDesc();

        hr = commandAllocator->Reset();
        assert(SUCCEEDED(hr));

        hr = commandList->Reset(commandAllocator.Get(), pipelineState.Get());
        assert(SUCCEEDED(hr));

        commandList->SetGraphicsRootSignature(rootSignature.Get());

        ID3D12DescriptorHeap* heaps[] = { uavHeap.Get(), samplerHeap.Get() };
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);

        commandList->SetGraphicsRootDescriptorTable(0, uavHeap->GetGPUDescriptorHandleForHeapStart());

        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissorRect);

        D3D12_RESOURCE_BARRIER backbuffer = {};
        backbuffer.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        backbuffer.Transition.pResource = rendertargets[frameIndex].Get();
        backbuffer.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        backbuffer.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        backbuffer.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commandList->ResourceBarrier(1, &backbuffer);

        auto rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvDesc = {};
        rtvDesc.ptr = rtvHandle.ptr + (descriptorSize * frameIndex);
        commandList->OMSetRenderTargets(1, &rtvDesc, false, nullptr);

        const float clearColor[] = { 1.f, 0.f, 1.f, 1.f };
        commandList->ClearRenderTargetView(rtvDesc, clearColor, 0, nullptr);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        commandList->DrawInstanced(6, 1, 0, 0);

        D3D12_RESOURCE_BARRIER present = {};
        present.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        present.Transition.pResource = rendertargets[frameIndex].Get();
        present.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        present.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        present.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        commandList->ResourceBarrier(1, &present);

        hr = commandList->Close();
        assert(SUCCEEDED(hr));

        ID3D12CommandList* commandLists[] = { commandList.Get() };
        commandQueue->ExecuteCommandLists(1, commandLists);
        hr = swapchain->Present(1, 0);
        assert(SUCCEEDED(hr));
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
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_D3D12VIEWER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_D3D12VIEWER);
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
