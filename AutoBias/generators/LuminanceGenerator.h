#pragma once

#include "../bias_util.h"

namespace hvk
{
	namespace d3d12
	{
		class LuminanceGenerator
		{
		public:
			LuminanceGenerator(ComPtr<ID3D12Device> device);
			HRESULT Generate(
				ComPtr<ID3D12GraphicsCommandList> commandList,
				ComPtr<ID3D12CommandQueue> commandQueue,
				ComPtr<ID3D12Resource> sourceTexture,
				uint32_t sourceMip,
				ComPtr<ID3D12Resource> luminanceTexture);

		private:
			ComPtr<ID3D12Device> mDevice;
			ComPtr<ID3D12RootSignature> mRootSig;
			ComPtr<ID3D12PipelineState> mPipelineState;
			ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;
		};
	}
}

