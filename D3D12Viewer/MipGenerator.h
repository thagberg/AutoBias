#pragma once

#include "bias_util.h"

namespace hvk
{
	namespace d3d12
	{
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
				ComPtr<ID3D12DescriptorHeap> uavHeap,
				uint8_t numMips,
				uint8_t startingMip,
				ComPtr<ID3D12Resource> sourceTexture,
				ComPtr<ID3D12Resource> mippedTexture);

		private:
			ComPtr<ID3D12Device> mDevice;
			ComPtr<ID3D12RootSignature> mRootSig;
			ComPtr<ID3D12PipelineState> mPipelineState;
			ComPtr<ID3D12Resource> mConstantBuffer;
		};
	}
}


