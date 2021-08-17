#include "generator/LuminanceGenerator.h"
#include <ShaderService.h>

namespace hvk
{
	namespace d3d12
	{
		LuminanceGenerator::LuminanceGenerator(ComPtr<ID3D12Device> device)
			: mDevice(device)
		{
			assert(mDevice != nullptr);

			//std::vector<uint8_t> luminanceByteCode;
			IDxcBlob* luminanceByteCode;
			//bool shaderLoadSuccess = bias::LoadShaderByteCode(L"shaders\\luminance.cso", luminanceByteCode);
			//assert(shaderLoadSuccess);
			hvk::render::shader::ShaderDefinition luminanceDef{
				L"shaders\\luminance.hlsl",
				L"main",
				L"cs_6_3"
			};
			hvk::render::shader::ShaderService::Initialize()->CompileShader(luminanceDef, &luminanceByteCode);

			// create descriptor heap
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.NumDescriptors = 2;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			HRESULT hr = mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDescriptorHeap));
			assert(SUCCEEDED(hr));

			D3D12_DESCRIPTOR_RANGE lumSrvRange = {};
			lumSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			lumSrvRange.NumDescriptors = 1;
			lumSrvRange.BaseShaderRegister = 0;
			lumSrvRange.RegisterSpace = 0;
			lumSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_DESCRIPTOR_RANGE lumUavRange = {};
			lumUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			lumUavRange.NumDescriptors = 1;
			lumUavRange.BaseShaderRegister = 0;
			lumUavRange.RegisterSpace = 0;
			lumUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_DESCRIPTOR_RANGE ranges[] = { lumSrvRange, lumUavRange };

			D3D12_ROOT_PARAMETER lumParam = {};
			lumParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			lumParam.DescriptorTable.NumDescriptorRanges = _countof(ranges);
			lumParam.DescriptorTable.pDescriptorRanges = ranges;
			lumParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			hr = render::CreateRootSignature(device, { lumParam }, {}, mRootSig);
			assert(SUCCEEDED(hr));

			//hr = hvk::render::CreateComputePipelineState(device, mRootSig, luminanceByteCode.data(), luminanceByteCode.size(), mPipelineState);
			hr = hvk::render::CreateComputePipelineState(
				device, 
				mRootSig, 
				reinterpret_cast<uint8_t*>(luminanceByteCode->GetBufferPointer()),
				luminanceByteCode->GetBufferSize(),
				mPipelineState);
			assert(SUCCEEDED(hr));
		}

		HRESULT LuminanceGenerator::Generate(
			ComPtr<ID3D12GraphicsCommandList> commandList,
			ComPtr<ID3D12CommandQueue> commandQueue,
			ComPtr<ID3D12Resource> sourceTexture,
			uint32_t sourceMip,
			ComPtr<ID3D12Resource> luminanceTexture)
		{
			HRESULT hr = S_OK;
			auto heapHandle = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			auto gpuHandle = mDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

			// create SRV for source texture
			D3D12_RESOURCE_DESC srcDesc = sourceTexture->GetDesc();
			D3D12_SHADER_RESOURCE_VIEW_DESC srcViewDesc = {};
			srcViewDesc.Format = srcDesc.Format;
			srcViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srcViewDesc.Texture2D.MipLevels = 1;
			srcViewDesc.Texture2D.MostDetailedMip = sourceMip;
			srcViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			mDevice->CreateShaderResourceView(sourceTexture.Get(), &srcViewDesc, heapHandle);
			heapHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			// create UAV for luminance texture
			D3D12_UNORDERED_ACCESS_VIEW_DESC lumUav = {};
			lumUav.Format = DXGI_FORMAT_A8_UNORM;
			lumUav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			lumUav.Texture2D.MipSlice = 0;
			mDevice->CreateUnorderedAccessView(luminanceTexture.Get(), nullptr, &lumUav, heapHandle);
			heapHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			// prepare for compute dispatch
			uint32_t dispatchX = (srcDesc.Width >> sourceMip) / 8;
			uint32_t dispatchY = (srcDesc.Height >> sourceMip) / 8;
			commandList->SetPipelineState(mPipelineState.Get());
			commandList->SetComputeRootSignature(mRootSig.Get());
			ID3D12DescriptorHeap* lumHeaps[] = { mDescriptorHeap.Get() };
			commandList->SetDescriptorHeaps(_countof(lumHeaps), lumHeaps);
			commandList->SetComputeRootDescriptorTable(0, gpuHandle);
			commandList->Dispatch(dispatchX, dispatchY, 1);

			commandList->Close();
			ID3D12CommandList* lumLists[] = { commandList.Get() };
			commandQueue->ExecuteCommandLists(1, lumLists);
			hr = render::WaitForGraphics(mDevice, commandQueue);
			assert(SUCCEEDED(hr));

			return hr;
		}
	}
}
