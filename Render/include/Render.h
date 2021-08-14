#pragma once

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl\client.h>
#include <exception>
#include <cstdint>
#include <cassert>
#include <vector>

using namespace Microsoft::WRL;

namespace hvk
{
	namespace render
	{
		void GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter);

		HRESULT CreateFactory(ComPtr<IDXGIFactory4>& factoryOut);

		HRESULT CreateDevice(ComPtr<IDXGIFactory4> factory, ComPtr<IDXGIAdapter1> hardwareAdapter, ComPtr<ID3D12Device>& deviceOut);

		HRESULT CreateCommandQueue(ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue>& cqOut);

		HRESULT CreateSwapchain(
			ComPtr<ID3D12CommandQueue> commandQueue,
			ComPtr<IDXGIFactory4> factory,
			HWND window,
			uint8_t bufferCount,
			uint16_t width,
			uint16_t height,
			ComPtr<IDXGISwapChain3>& scOut);

		HRESULT CreateDescriptorHeaps(
			ComPtr<ID3D12Device> device,
			uint8_t framebufferCount,
			ComPtr<ID3D12DescriptorHeap>& rtvOut,
			ComPtr<ID3D12DescriptorHeap>& miscOut,
			ComPtr<ID3D12DescriptorHeap>& samplersOut);

		HRESULT CreateRenderTargetView(
			ComPtr<ID3D12Device> device,
			ComPtr<IDXGISwapChain1> swapchain,
			ComPtr<ID3D12DescriptorHeap> rtvHeap,
			size_t numRendertargets,
			ComPtr<ID3D12Resource>* rtvOut);

		HRESULT CreateCommandAllocator(ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandAllocator>& caOut);

		HRESULT CreateRootSignature(
			ComPtr<ID3D12Device> device,
			const std::vector<D3D12_ROOT_PARAMETER>& rootParams,
			const std::vector<D3D12_STATIC_SAMPLER_DESC>& samplers,
			ComPtr<ID3D12RootSignature>& rsOut);

		HRESULT CreateGraphicsPipelineState(
			ComPtr<ID3D12Device> device,
			D3D12_INPUT_LAYOUT_DESC& inputLayout,
			ComPtr<ID3D12RootSignature> rootSig,
			const uint8_t* vertexShader,
			size_t vertexShaderSize,
			const uint8_t* pixelShader,
			size_t pixelShaderSize,
			ComPtr<ID3D12PipelineState>& psOut);

		HRESULT CreateComputePipelineState(
			ComPtr<ID3D12Device> device,
			ComPtr<ID3D12RootSignature> rootSig,
			const uint8_t* computeShader,
			size_t computeShaderSize,
			ComPtr<ID3D12PipelineState>& psOut);

		HRESULT CreateCommandList(
			ComPtr<ID3D12Device> device,
			ComPtr<ID3D12CommandAllocator> allocator,
			ComPtr<ID3D12PipelineState> pipeline,
			ComPtr<ID3D12GraphicsCommandList>& clOut);

		HRESULT CreateFence(ComPtr<ID3D12Device> device, ComPtr<ID3D12Fence>& fOut);

		D3D12_VERTEX_BUFFER_VIEW CreateVertexBuffer(ComPtr<ID3D12Device> device, const uint8_t* vbData, uint32_t vbSize, uint32_t stride, ComPtr<ID3D12Resource>& vbOut);

		HRESULT WaitForGraphics(ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue> commandQueue);

		D3D12_HEAP_PROPERTIES HeapPropertiesDefault();

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
			D3D12_HEAP_TYPE heaptype,
			ComPtr<ID3D12Resource>& outResource);
	}
}
