// Render.cpp : Defines the functions for the static library.
//

#include "pch.h"
#include "framework.h"
#include "Render.h"

// TODO: This is an example of a library function
void fnRender()
{
}

#if !defined(RENDER_UTIL)
#define RENDER_UTIL

namespace hvk
{
	namespace render
	{
		const uint16_t kMiscDescriptors = 16;
		const uint16_t kSamplerDescriptors = 16;

		void GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter)
		{
			ComPtr<IDXGIAdapter1> adapter;
			*ppAdapter = nullptr;

			for (uint8_t i = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(i, &adapter); ++i)
			{
				DXGI_ADAPTER_DESC1 desc;
				adapter->GetDesc1(&desc);

				// Skip software adapter (WARP)
				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					continue;
				}

				// break on the first hardware adapter
				// TODO: Probably want to ensure it supports D3D12
				break;
			}

			*ppAdapter = adapter.Detach();
		}

		HRESULT CreateFactory(ComPtr<IDXGIFactory4>& factoryOut)
		{
			auto hr = S_OK;

			hr = CreateDXGIFactory1(IID_PPV_ARGS(&factoryOut));

			return hr;
		}

		HRESULT CreateDevice(ComPtr<IDXGIFactory4> factory, ComPtr<IDXGIAdapter1> hardwareAdapter, ComPtr<ID3D12Device>& deviceOut)
		{
#if defined(_DEBUG)
			// Enable debug layers
			{
				ComPtr<ID3D12Debug1> debugController;
				assert(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))));
			}
#endif

			HRESULT hr = S_OK;

			hr = D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&deviceOut));
			return hr;
		}

		HRESULT CreateCommandQueue(ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue>& cqOut)
		{
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			auto hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cqOut));
			return hr;
		}

		HRESULT CreateSwapchain(
			ComPtr<ID3D12CommandQueue> commandQueue,
			ComPtr<IDXGIFactory4> factory,
			HWND window,
			uint8_t bufferCount, 
			uint16_t width, 
			uint16_t height, 
			ComPtr<IDXGISwapChain3>& scOut)
		{
			DXGI_SWAP_CHAIN_DESC1 desc = {};
			desc.BufferCount = bufferCount;
			desc.Width = width;
			desc.Height = height;
			desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.Scaling = DXGI_SCALING_STRETCH;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;

			// Using nullptr for the FULLSCREEN_DESC to ensure this is windowed
			ComPtr<IDXGISwapChain1> swap1;
			auto hr = factory->CreateSwapChainForHwnd(
				commandQueue.Get(),
				window,
				&desc,
				nullptr,
				nullptr,
				&swap1);
			assert(SUCCEEDED(hr));
			hr = swap1->QueryInterface<IDXGISwapChain3>(&scOut);
			return hr;
		}

		HRESULT CreateDescriptorHeaps(
			ComPtr<ID3D12Device> device, 
			uint8_t framebufferCount, 
			ComPtr<ID3D12DescriptorHeap>& rtvOut,
			ComPtr<ID3D12DescriptorHeap>& miscOut,
			ComPtr<ID3D12DescriptorHeap>& samplersOut)
		{
			auto hr = S_OK;

			D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
			rtvDesc.NumDescriptors = framebufferCount;
			rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			hr = device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rtvOut));
			if (hr != S_OK)
			{
				return hr;
			}

			D3D12_DESCRIPTOR_HEAP_DESC miscDesc = {};
			miscDesc.NumDescriptors = kMiscDescriptors;
			miscDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			miscDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			hr = device->CreateDescriptorHeap(&miscDesc, IID_PPV_ARGS(&miscOut));
			if (hr != S_OK)
			{
				return hr;
			}

			D3D12_DESCRIPTOR_HEAP_DESC samplersDesc = {};
			samplersDesc.NumDescriptors = kSamplerDescriptors;
			samplersDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			samplersDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			hr = device->CreateDescriptorHeap(&samplersDesc, IID_PPV_ARGS(&samplersOut));

			return hr;
		}

		HRESULT CreateRenderTargetView(
			ComPtr<ID3D12Device> device, 
			ComPtr<IDXGISwapChain1> swapchain,
			ComPtr<ID3D12DescriptorHeap> rtvHeap,
			size_t numRendertargets,
			ComPtr<ID3D12Resource>* rtvOut)
		{
			auto hr = S_OK;
			auto rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

			auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			for (size_t i = 0; i < numRendertargets; ++i)
			{
				// TODO: provide D3D12_RENDER_TARGET_VIEW_DESC instead of nullptr
				hr = swapchain->GetBuffer(static_cast<uint32_t>(i), IID_PPV_ARGS(&rtvOut[i]));
				if (SUCCEEDED(hr))
				{
					device->CreateRenderTargetView(rtvOut[i].Get(), nullptr, rtvHandle);
					rtvHandle.ptr += descriptorSize;
				}
				else
				{
					break;
				}
			}
			return hr;
		}

		HRESULT CreateCommandAllocator(ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandAllocator>& caOut)
		{
			auto hr = S_OK;

			hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&caOut));

			return hr;
		}

		HRESULT CreateRootSignature(
			ComPtr<ID3D12Device> device, 
			const std::vector<D3D12_ROOT_PARAMETER>& rootParams,
			const std::vector<D3D12_STATIC_SAMPLER_DESC>& samplers,
			ComPtr<ID3D12RootSignature>& rsOut)
		{
			auto hr = S_OK;

			D3D12_ROOT_SIGNATURE_DESC desc = {};
			desc.NumParameters = static_cast<uint8_t>(rootParams.size());
			desc.pParameters = rootParams.data();
			desc.NumStaticSamplers = static_cast<uint8_t>(samplers.size());
			desc.pStaticSamplers = samplers.data();
			desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
			hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rsOut));

			return hr;
		}

		HRESULT CreateGraphicsPipelineState(
			ComPtr<ID3D12Device> device, 
			D3D12_INPUT_LAYOUT_DESC& inputLayout,
			ComPtr<ID3D12RootSignature> rootSig,
			const uint8_t* vertexShader,
			size_t vertexShaderSize,
			const uint8_t* pixelShader,
			size_t pixelShaderSize,
			ComPtr<ID3D12PipelineState>& psOut)
		{
			auto hr = S_OK;

			D3D12_RENDER_TARGET_BLEND_DESC rtBlend = {};
			rtBlend.BlendEnable = FALSE;
			rtBlend.LogicOpEnable = FALSE;
			rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			D3D12_BLEND_DESC blend = {};
			blend.AlphaToCoverageEnable = FALSE;
			blend.IndependentBlendEnable = FALSE;
			for (uint8_t i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
			{
				blend.RenderTarget[i] = rtBlend;
			}

			D3D12_RASTERIZER_DESC raster = {};
			raster.FillMode = D3D12_FILL_MODE_SOLID;
			raster.CullMode = D3D12_CULL_MODE_NONE;
			raster.FrontCounterClockwise = FALSE;
			raster.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
			raster.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
			raster.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
			raster.DepthClipEnable = TRUE;
			raster.MultisampleEnable = FALSE;
			raster.AntialiasedLineEnable = FALSE;
			raster.ForcedSampleCount = 0;
			raster.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

			D3D12_DEPTH_STENCIL_DESC depthStencil = {};
			depthStencil.DepthEnable = FALSE;
			depthStencil.StencilEnable = FALSE;

			D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
			desc.InputLayout = inputLayout;
			desc.pRootSignature = rootSig.Get();
			desc.VS = { vertexShader, vertexShaderSize };
			desc.PS = { pixelShader, pixelShaderSize };
			desc.RasterizerState = raster;
			desc.BlendState = blend;
			desc.SampleMask = UINT_MAX; // TODO: learn more about this
			desc.DepthStencilState = depthStencil;
			desc.NodeMask = 0;
			desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			desc.NumRenderTargets = 1;
			desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;

			hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&psOut));

			return hr;
		}

		HRESULT CreateComputePipelineState(
			ComPtr<ID3D12Device> device, 
			ComPtr<ID3D12RootSignature> rootSig,
			const uint8_t* computeShader,
			size_t computeShaderSize,
			ComPtr<ID3D12PipelineState>& psOut)
		{
			auto hr = S_OK;

			D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
			desc.CS = { computeShader, computeShaderSize };
			desc.NodeMask = 0;
			desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
			desc.pRootSignature = rootSig.Get();

			hr = device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&psOut));

			return hr;
		}

		HRESULT CreateCommandList(
			ComPtr<ID3D12Device> device,
			ComPtr<ID3D12CommandAllocator> allocator,
			ComPtr<ID3D12PipelineState> pipeline,
			ComPtr<ID3D12GraphicsCommandList>& clOut)
		{
			auto hr = S_OK;

			hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), pipeline.Get(), IID_PPV_ARGS(&clOut));

			return hr;
		}

		HRESULT CreateFence(ComPtr<ID3D12Device> device, ComPtr<ID3D12Fence>& fOut)
		{
			auto hr = S_OK;

			hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fOut));

			return hr;
		}

		D3D12_VERTEX_BUFFER_VIEW CreateVertexBuffer(ComPtr<ID3D12Device> device, const uint8_t* vbData, uint32_t vbSize, uint32_t stride, ComPtr<ID3D12Resource>& vbOut)
		{
			auto hr = S_OK;

			D3D12_HEAP_PROPERTIES heapProps = {};
			heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProps.CreationNodeMask = 1;
			heapProps.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC vbDesc = {};
			vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			vbDesc.Alignment = 0;
			vbDesc.Width = vbSize;
			vbDesc.Height = 1;
			vbDesc.DepthOrArraySize = 1;
			vbDesc.MipLevels = 1;
			vbDesc.Format = DXGI_FORMAT_UNKNOWN;
			vbDesc.SampleDesc.Count = 1;
			vbDesc.SampleDesc.Quality = 0;
			vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			vbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			hr = device->CreateCommittedResource(
				&heapProps, 
				D3D12_HEAP_FLAG_NONE, 
				&vbDesc, 
				D3D12_RESOURCE_STATE_GENERIC_READ, 
				nullptr, 
				IID_PPV_ARGS(&vbOut));
			assert(SUCCEEDED(hr));

			D3D12_RANGE readRange = {};
			readRange.Begin = 0;
			readRange.End = 0;

			uint8_t* vbBegin;
			vbOut->Map(0, &readRange, reinterpret_cast<void**>(&vbBegin));
			memcpy(vbBegin, vbData, vbSize);
			vbOut->Unmap(0, nullptr);

			D3D12_VERTEX_BUFFER_VIEW vbView;
			vbView.BufferLocation = vbOut->GetGPUVirtualAddress();
			vbView.StrideInBytes = stride;
			vbView.SizeInBytes = vbSize;
			return vbView;
		}

		HRESULT WaitForGraphics(ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue> commandQueue)
		{
			HRESULT hr = S_OK;
			ComPtr<ID3D12Fence> fence;
			hr = render::CreateFence(device, fence);
			uint64_t fenceValue = 1;
			hr = commandQueue->Signal(fence.Get(), fenceValue);
			assert(SUCCEEDED(hr));

			if (SUCCEEDED(hr))
			{
				auto fenceEvent = CreateEvent(nullptr, false, false, nullptr);
				auto completedVal = fence->GetCompletedValue();
				if (completedVal != fenceValue)
				{
					hr = fence->SetEventOnCompletion(fenceValue, fenceEvent);
					assert(SUCCEEDED(hr));
					WaitForSingleObject(fenceEvent, INFINITE);
				}
			}

			return hr;
		}

		D3D12_HEAP_PROPERTIES HeapPropertiesDefault()
		{
			D3D12_HEAP_PROPERTIES props = {};
			props.Type = D3D12_HEAP_TYPE_DEFAULT;
			props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			props.CreationNodeMask = 1;
			props.VisibleNodeMask = 1;
			return props;
		}

		HRESULT CreateResource(
			ComPtr<ID3D12Device> device,
			D3D12_RESOURCE_DIMENSION resourceDimension,
			DXGI_FORMAT format,
			uint64_t width,
			uint64_t height,
			uint16_t depthOrArraySize,
			uint16_t numMips,
			D3D12_RESOURCE_FLAGS flags,
			D3D12_TEXTURE_LAYOUT layout,
			D3D12_RESOURCE_STATES resourceStates,
			D3D12_HEAP_TYPE heapType,
			ComPtr<ID3D12Resource>& outResource)
		{
			HRESULT hr = S_OK;

			D3D12_RESOURCE_DESC desc = {};
			desc.Dimension = resourceDimension;
			desc.Format = format;
			desc.Alignment = 0;
			desc.Width = width;
			desc.Height = height;
			desc.DepthOrArraySize = depthOrArraySize;
			desc.MipLevels = numMips;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Flags = flags;
			desc.Layout = layout;

			D3D12_HEAP_PROPERTIES heapProps = HeapPropertiesDefault();
			heapProps.Type = heapType;

			hr = device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				resourceStates,
				nullptr,
				IID_PPV_ARGS(&outResource));

			return hr;
		}
	}
}

#endif
