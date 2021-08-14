#pragma once

#include <cstdint>
#include <dxgi1_6.h>

#include "Render.h"

struct ID3D12Resource;
struct IDXGIResource;

template <typename T, typename U>
T Align(T offset, U alignment)
{
    return (offset + (alignment - 1)) & ~(alignment - 1);
}

namespace hvk
{
	namespace bias
	{
		struct MipConstantBuffer
		{
			uint32_t SrcMipLevel;
			uint32_t NumMipLevels;
			float TexelSize[2];
			uint32_t SrcDimensions;
		};

		bool LoadShaderByteCode(LPCWSTR filename, std::vector<uint8_t>& byteCodeOut);

		//HRESULT GetNextFrameResource(const CaptureManager& cm, IDXGIResource** outResource);

		HRESULT GenerateColorCorrectionLUT(
			ComPtr<ID3D12Device> device,
			ComPtr<ID3D12GraphicsCommandList> singleUse,
			ComPtr<ID3D12CommandQueue> commandQueue,
			ComPtr<ID3D12Resource> colorCorrectionTex);

		HRESULT GenerateMips(
			ComPtr<ID3D12Device> device,
			ComPtr<ID3D12GraphicsCommandList> singleUse,
			ComPtr<ID3D12CommandQueue> commandQueue,
			ComPtr<ID3D12DescriptorHeap> uavHeap,
			uint8_t numMips,
			uint8_t startingMip,
			ComPtr<ID3D12Resource> sourceTexture,
			ComPtr<ID3D12Resource> mippedTexture);

		HRESULT GenerateDummyTexture(
			ComPtr<ID3D12Device> device,
			ComPtr<ID3D12GraphicsCommandList> singleUseCommandList,
			ComPtr<ID3D12CommandQueue> commandQueue,
			ComPtr<ID3D12Resource>& d3d12Resource);

		HRESULT GenerateLuminanceMap(
			ComPtr<ID3D12Device> device,
			ComPtr<ID3D12GraphicsCommandList> singleUse,
			ComPtr<ID3D12CommandQueue> commandQueue,
			ComPtr<ID3D12DescriptorHeap> uavHeap,
			ComPtr<ID3D12Resource> sourceTexture,
			ComPtr<ID3D12Resource> luminanceTexture);
	}
}

