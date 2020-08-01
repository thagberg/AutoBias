#pragma once

#include "bias_util.h"
#include <array>

namespace hvk
{
	namespace d3d12
	{
		const size_t kMaxPasses = 4;

		struct MipConstantBuffer
		{
			uint32_t SrcMipLevel;
			uint32_t NumMipLevels;
			float TexelSize[2];
			uint32_t SrcDimensions;
		};

		class MipGenerator
		{
		public:
			MipGenerator(ComPtr<ID3D12Device> device);
			HRESULT Generate(
				ComPtr<ID3D12GraphicsCommandList> commandList,
				ComPtr<ID3D12CommandQueue> commandQueue,
				uint8_t numMips,
				uint8_t startingMip,
				ComPtr<ID3D12Resource> sourceTexture,
				ComPtr<ID3D12Resource> mippedTexture);

		private:
			ComPtr<ID3D12Device> mDevice;
			ComPtr<ID3D12RootSignature> mRootSig;
			ComPtr<ID3D12PipelineState> mPipelineState;
			std::array<ComPtr<ID3D12Resource>, kMaxPasses> mConstantBuffers;
			std::array<ComPtr<ID3D12DescriptorHeap>, kMaxPasses> mDescriptorHeaps;
		};
	}
}


