// D3D12Viewer.cpp : Defines the entry point for the application.
//

#define NOMINMAX 1

#include "framework.h"
#include "D3D12Viewer.h"

#include <iostream>
#include <memory>
#include <algorithm>

#include <d3d12.h>
#include <DirectXMath.h>

#include <Render.h>
#include <AutoBias.h>

#define CAPTURE
#if defined(CAPTURE)
#include <CaptureManager.h>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

//#define RENDERDOC
#if defined(RENDERDOC)
#include "renderdoc_app.h"
RENDERDOC_API_1_4_1* rdoc_api = nullptr;
#endif

#define MAX_LOADSTRING 100

const uint8_t kGridWidth = 26;
const uint8_t kGridHeight =  18;
const size_t kFramebuffers = 2;

struct Vertex
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 TexCoord;
};

const uint8_t kNumVertices = 6;
const Vertex kVertices[kNumVertices] =
{
	{DirectX::XMFLOAT3(-1.f, -1.f, 0.5f), DirectX::XMFLOAT2(0.f, 1.f)},
	{DirectX::XMFLOAT3(-1.f, 1.f, 0.5f), DirectX::XMFLOAT2(0.f, 0.f)},
	{DirectX::XMFLOAT3(1.f, -1.f, 0.5f), DirectX::XMFLOAT2(1.f, 1.f)},
	{DirectX::XMFLOAT3(1.f, -1.f, 0.5f), DirectX::XMFLOAT2(1.f, 1.f)},
	{DirectX::XMFLOAT3(-1.f, 1.f, 0.5f), DirectX::XMFLOAT2(0.f, 0.f)},
	{DirectX::XMFLOAT3(1.f, 1.f, 0.5f), DirectX::XMFLOAT2(1.f, 0.f)},

};

// TODO: mask should either be from file or another runtime input
//const int kLedMask[] = {
//    -1, -1, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, -1, -1,
//    -1, 45, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 22, -1,
//    46, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 21,
//    47, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 20,
//    48, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 19,
//    49, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 18,
//    50, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 17,
//    51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16,
//    52, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 15,
//    53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 14,
//    54, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 13,
//    55, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 12,
//    56, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 11,
//    57, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10,
//    58, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  9,
//    59, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  8,
//    -1, 60, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  7, -1,
//    -1, -1, 61, 62, 63, 64, 65, 66, 67, -1, -1, -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6, -1, -1
//};

const std::vector<int> kLedMask = {
    -1, -1, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, -1, -1,
    -1, 45, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 22, -1,
    46, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 21,
    47, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 20,
    48, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 19,
    49, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 18,
    50, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 17,
    51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16,
    52, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 15,
    53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 14,
    54, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 13,
    55, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 12,
    56, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 11,
    57, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10,
    58, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  9,
    59, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  8,
    -1, 60, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  7, -1,
    -1, -1, 61, 62, 63, 64, 65, 66, 67, -1, -1, -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6, -1, -1
};

//const int kLedMask[] = {
//     0,  1,  2,  3,  4,  5,  6,  7,
//     8,  9, 10, 11, 12, 13, 14, 15,
//    16, 17, 18, 19, 20, 21, 22, 23,
//    24, 25, 26, 27, 28, 29, 30, 31,
//    32, 33, 34, 35, 36, 37, 38, 39
//};


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
#if defined(RENDERDOC)
	HMODULE renderMod = LoadLibraryA("renderdoc.dll");
	if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
	{
		pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
		int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_1, (void**)&rdoc_api);
		assert(ret == 1);
	}
#endif
    
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
    ComPtr<IDXGISwapChain3> swapchain;
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
    bool shaderLoadSuccess = hvk::bias::LoadShaderByteCode(L"shaders\\vertex.cso", vertexByteCode);
    shaderLoadSuccess &= hvk::bias::LoadShaderByteCode(L"shaders\\pixel.cso", pixelByteCode);
    assert(shaderLoadSuccess);

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0.f;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    sampler.MinLOD = 0.f;
    sampler.MaxLOD = 100.f;
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
    vertexByteCode.clear();
    pixelByteCode.clear();

    ComPtr<ID3D12CommandAllocator> commandAllocator;
    hr = hvk::render::CreateCommandAllocator(device, commandAllocator);
    assert(SUCCEEDED(hr));

    ComPtr<ID3D12CommandAllocator> singleUseAllocator;
    hr = hvk::render::CreateCommandAllocator(device, singleUseAllocator);
    assert(SUCCEEDED(hr));

    ComPtr<ID3D12GraphicsCommandList> commandList;
    hr = hvk::render::CreateCommandList(device, commandAllocator, pipelineState, commandList);
    assert(SUCCEEDED(hr));
    commandList->Close();

	// create single-use command list
	ComPtr<ID3D12GraphicsCommandList> singleUse;
	hr = hvk::render::CreateCommandList(device, singleUseAllocator, nullptr, singleUse);
	assert(SUCCEEDED(hr));
    singleUse->Close();

    ComPtr<ID3D12Resource> rendertargets[kFramebuffers];
    hr = hvk::render::CreateRenderTargetView(device, swapchain, rtvHeap, kFramebuffers, rendertargets);
    assert(SUCCEEDED(hr));

    ComPtr<ID3D12Fence> frameFence;
    uint64_t fenceValue = 0;
    hr = hvk::render::CreateFence(device, frameFence);
    auto fenceEvent = CreateEvent(nullptr, false, false, nullptr);
    assert(SUCCEEDED(hr));

    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(windowWidth);
    viewport.Height = static_cast<float>(windowHeight);
    viewport.TopLeftX = 0.f;
    viewport.TopLeftY = 0.f;
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;

    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.right = windowWidth;
    scissorRect.top = 0;
    scissorRect.bottom = windowHeight;

	ComPtr<ID3D12Resource> d3d12Resource;

#if defined(CAPTURE)
    CaptureManager cm;
    hr = cm.Init();
#else
    commandList->Reset(commandAllocator.Get(), nullptr);
    hr = hvk::bias::GenerateDummyTexture(device, commandList, commandQueue, d3d12Resource);
    assert(SUCCEEDED(hr));
#endif

    // create LED preview texture
    ComPtr<ID3D12Resource> previewTexture;

    D3D12_HEAP_PROPERTIES previewHeap = {};
    previewHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    previewHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    previewHeap.CreationNodeMask = 1;
    previewHeap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC previewDesc = {};
    previewDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    previewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    previewDesc.MipLevels = 1;
    previewDesc.Width = kGridWidth;
    previewDesc.Height = kGridHeight;
    previewDesc.DepthOrArraySize = 1;
    previewDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    previewDesc.SampleDesc.Count = 1;
    previewDesc.SampleDesc.Quality = 0;

    hr = device->CreateCommittedResource(
        &previewHeap,
        D3D12_HEAP_FLAG_NONE,
        &previewDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(&previewTexture));
    assert(SUCCEEDED(hr));

    // create AutoBias
    hvk::bias::AutoBias ab(device, kGridWidth, kGridHeight, kLedMask, desktopWidth, desktopHeight);

    //------------- Application Loop ---------------

    MSG msg;
    bool running = true;

    IDXGIResource* desktopResource = nullptr;
	D3D12_RESOURCE_DESC resourceDesc = {};

    uint8_t frameIndex = 0;
    uint64_t frameCount = 0;
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

#if defined(CAPTURE)
        //hr = hvk::bias::GetNextFrameResource(cm, &desktopResource);
		hr = cm.AcquireFrameAsDXGIResource(&desktopResource);
		if (!SUCCEEDED(hr))
		{
			uint8_t numRetries = 0;
			while (numRetries < 10)
			{
				++numRetries;
				Sleep(200);
				//cm = CaptureManager();
				//hr = cm.Init();
				if (SUCCEEDED(hr))
				{
					hr = cm.AcquireFrameAsDXGIResource(&desktopResource);
					if (SUCCEEDED(hr))
					{
						break;
					}
				}
			}
		}
        // If we keep failing to acquire the desktop image, the user might be
        // in a UAC prompt, Ctrl+Alt+Del, etc., so just skip the rest of this frame
        // and try again later
        if (!SUCCEEDED(hr))
        {
            continue;
        }

        IDXGIResource1* sharedResource;
        desktopResource->QueryInterface<IDXGIResource1>(&sharedResource);
        HANDLE hTexture;
        sharedResource->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &hTexture);
        device->OpenSharedHandle(hTexture, IID_PPV_ARGS(&d3d12Resource));
        resourceDesc = d3d12Resource->GetDesc();
#else
        resourceDesc = d3d12Resource->GetDesc();
#endif

#if defined(RENDERDOC)
        if (frameCount == 0)
        {
			rdoc_api->StartFrameCapture(device.Get(), window);
        }
#endif

        ab.Update(d3d12Resource);

        // generate preview texture from LED buffer
        {
			const auto ledBuffer = ab.GetLEDBuffer();

			commandList->Reset(commandAllocator.Get(), nullptr);

            D3D12_RANGE ledRange = { 0, 0 };
            uint8_t* ledPtr;
            ledBuffer->Map(0, &ledRange, reinterpret_cast<void**>(&ledPtr));
            {
				D3D12_HEAP_PROPERTIES intrHeap = {};
				intrHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
				intrHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				intrHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				intrHeap.CreationNodeMask = 1;
				intrHeap.VisibleNodeMask = 1;

				ComPtr<ID3D12Resource> intr;
				D3D12_RESOURCE_DESC intrDesc = {};
				intrDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				intrDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
				intrDesc.Width = Align(kGridWidth * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * kGridHeight - (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - (kGridWidth * 4));
				intrDesc.Height = 1;
				intrDesc.DepthOrArraySize = 1;
				intrDesc.MipLevels = 1;
				intrDesc.Format = DXGI_FORMAT_UNKNOWN;
				intrDesc.SampleDesc.Count = 1;
				intrDesc.SampleDesc.Quality = 0;
				intrDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				intrDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

				device->CreateCommittedResource(
					&intrHeap,
					D3D12_HEAP_FLAG_NONE,
					&intrDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&intr));

				uint8_t* intrData;
				intr->Map(0, nullptr, reinterpret_cast<void**>(&intrData));
				uint8_t* writeAt = intrData;
				for (size_t ledY = 0; ledY < kGridHeight; ++ledY)
				{
					size_t sourceOffset = (ledY * kGridWidth * 4);
                    memcpy(writeAt, ledPtr + sourceOffset, kGridWidth * 4);
                    writeAt = reinterpret_cast<uint8_t*>(Align(reinterpret_cast<size_t>(writeAt)+1, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
				}
				intr->Unmap(0, nullptr);

				D3D12_TEXTURE_COPY_LOCATION previewSrc = {};
				previewSrc.pResource = intr.Get();
				previewSrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

				uint64_t requiredSize = 0;
				device->GetCopyableFootprints(&previewTexture->GetDesc(), 0, 1, 0, &previewSrc.PlacedFootprint, nullptr, nullptr, &requiredSize);

				D3D12_RESOURCE_BARRIER cpyBarrier = {};
				cpyBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				cpyBarrier.Transition.pResource = previewTexture.Get();
				cpyBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				cpyBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				cpyBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
				commandList->ResourceBarrier(1, &cpyBarrier);


				D3D12_TEXTURE_COPY_LOCATION previewDest = {};
				previewDest.pResource = previewTexture.Get();
				previewDest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				previewDest.SubresourceIndex = 0;
				commandList->CopyTextureRegion(&previewDest, 0, 0, 0, &previewSrc, nullptr);


				D3D12_RESOURCE_BARRIER previewBarrier = {};
				previewBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				previewBarrier.Transition.pResource = previewTexture.Get();
				previewBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				previewBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
				previewBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				commandList->ResourceBarrier(1, &previewBarrier);
				commandList->Close();
				ID3D12CommandList* previewList[] = { commandList.Get() };
				commandQueue->ExecuteCommandLists(1, previewList);
				hvk::render::WaitForGraphics(device, commandQueue);
            }
            ledBuffer->Unmap(0, nullptr);
        }

        hr = commandAllocator->Reset();
        assert(SUCCEEDED(hr));

        hr = commandList->Reset(commandAllocator.Get(), pipelineState.Get());
        assert(SUCCEEDED(hr));

        //D3D12_RESOURCE_DESC mippedDesc = mippedTexture->GetDesc();
        D3D12_RESOURCE_DESC previewDesc = previewTexture->GetDesc();
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		//srvDesc.Format = mippedDesc.Format;
		srvDesc.Format = previewDesc.Format;
        //srvDesc.Format = resourceDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		//srvDesc.Texture2D.MipLevels = mippedDesc.MipLevels;
		srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        auto resourcePtr = previewTexture.Get();
        auto uavHandle = uavHeap->GetCPUDescriptorHandleForHeapStart();
        device->CreateShaderResourceView(resourcePtr, &srvDesc, uavHandle);

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

        // mip resource barrier
        //D3D12_RESOURCE_BARRIER mipBarrier = {};
        //mipBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        //mipBarrier.Transition.pResource = mippedTexture.Get();
        //mipBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        //mipBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        //mipBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        //commandList->ResourceBarrier(1, &mipBarrier);

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

#if defined(RENDERDOC)
        if (frameCount == 0)
        {
			rdoc_api->EndFrameCapture(device.Get(), window);
        }
#endif


        uint64_t fence = fenceValue;
        commandQueue->Signal(frameFence.Get(), fence);
        ++fenceValue;

        std::cout << frameFence->GetCompletedValue() << std::endl;
        if (frameFence->GetCompletedValue() < fence)
        {
            hr = frameFence->SetEventOnCompletion(fence, fenceEvent);
            assert(SUCCEEDED(hr));
			WaitForSingleObject(fenceEvent, INFINITE);
		}

        frameIndex = swapchain->GetCurrentBackBufferIndex();
        ++frameCount;

        Sleep(16);
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
       auto err = GetLastError();
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
