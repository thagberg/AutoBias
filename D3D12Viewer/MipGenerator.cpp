#define NOMINMAX 1

#include "MipGenerator.h"
#include "bias_util.h"

#include <algorithm>

const LPCWSTR kMipShaderFile = L"shaders\\generate_mips.cso";

namespace hvk
{
	namespace d3d12
	{
		MipGenerator::MipGenerator(ComPtr<ID3D12Device> device)
			: mDevice(device)
		{
			assert(mDevice != nullptr);

			std::vector<uint8_t> mipByteCode;
			bool shaderLoadSuccess = bias::LoadShaderByteCode(kMipShaderFile, mipByteCode);
			assert(shaderLoadSuccess);

			D3D12_DESCRIPTOR_RANGE mipUavRange = {};
			mipUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			mipUavRange.NumDescriptors = 4;
			mipUavRange.BaseShaderRegister = 0;
			mipUavRange.RegisterSpace = 0;
			mipUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_DESCRIPTOR_RANGE mipSrvRange = {};
			mipSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			mipSrvRange.NumDescriptors = 1;
			mipSrvRange.BaseShaderRegister = 0;
			mipSrvRange.RegisterSpace = 0;
			mipSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_DESCRIPTOR_RANGE mipCbRange = {};
			mipCbRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			mipCbRange.NumDescriptors = 1;
			mipCbRange.BaseShaderRegister = 0;
			mipCbRange.RegisterSpace = 0;
			mipCbRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_DESCRIPTOR_RANGE ranges[] = { mipUavRange, mipSrvRange, mipCbRange };

			D3D12_ROOT_PARAMETER mipParam = {};
			mipParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			mipParam.DescriptorTable.NumDescriptorRanges = _countof(ranges);
			mipParam.DescriptorTable.pDescriptorRanges = ranges;
			mipParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			D3D12_STATIC_SAMPLER_DESC bilinearSampler = {};
			bilinearSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			bilinearSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			bilinearSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			bilinearSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			bilinearSampler.MipLODBias = 0.f;
			bilinearSampler.MaxAnisotropy = 1;
			bilinearSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			bilinearSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
			bilinearSampler.MinLOD = 0;
			bilinearSampler.MaxLOD = 0;
			bilinearSampler.ShaderRegister = 0;
			bilinearSampler.RegisterSpace = 0;
			bilinearSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			auto hr = render::CreateRootSignature(mDevice, { mipParam }, { bilinearSampler }, mRootSig);
			assert(SUCCEEDED(hr));

			hr = render::CreateComputePipelineState(mDevice, mRootSig, mipByteCode.data(), mipByteCode.size(), mPipelineState);
			assert(SUCCEEDED(hr));

			// create constant buffer for mip generation
			D3D12_RESOURCE_DESC cbDesc = {};
			cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			cbDesc.Format = DXGI_FORMAT_UNKNOWN;
			cbDesc.Alignment = 0;
			cbDesc.Width = (sizeof(MipConstantBuffer) + 255) & ~255;
			cbDesc.Height = 1;
			cbDesc.DepthOrArraySize = 1;
			cbDesc.MipLevels = 1;
			cbDesc.SampleDesc.Count = 1;
			cbDesc.SampleDesc.Quality = 0;
			cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			cbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			auto cbHeapProps = hvk::render::HeapPropertiesDefault();
			cbHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
			hr = mDevice->CreateCommittedResource(&cbHeapProps, D3D12_HEAP_FLAG_NONE, &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mConstantBuffer));
			assert(SUCCEEDED(hr));
		}

		HRESULT MipGenerator::Generate(
			ComPtr<ID3D12GraphicsCommandList> commandList,
			ComPtr<ID3D12CommandQueue> commandQueue,
			ComPtr<ID3D12DescriptorHeap> uavHeap,
			uint8_t numMips,
			uint8_t startingMip,
			ComPtr<ID3D12Resource> sourceTexture,
			ComPtr<ID3D12Resource> mippedTexture)
		{
			HRESULT hr = S_OK;

			// copy the source texture to mip 0 of the mipmap resource
			D3D12_RESOURCE_BARRIER preCpyBarrier = {};
			preCpyBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			preCpyBarrier.Transition.pResource = mippedTexture.Get();
			preCpyBarrier.Transition.Subresource = 0;
			preCpyBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			preCpyBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
			commandList->ResourceBarrier(1, &preCpyBarrier);

			D3D12_RESOURCE_DESC srcDesc = sourceTexture->GetDesc();
			D3D12_TEXTURE_COPY_LOCATION cpySource = {};
			cpySource.pResource = sourceTexture.Get();
			cpySource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			uint64_t requiredSize = 0;
			mDevice->GetCopyableFootprints(&srcDesc, 0, 1, 0, &cpySource.PlacedFootprint, nullptr, nullptr, &requiredSize);

			D3D12_TEXTURE_COPY_LOCATION cpyDest = {};
			cpyDest.pResource = mippedTexture.Get();
			cpyDest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			cpyDest.SubresourceIndex = 0;
			commandList->CopyTextureRegion(&cpyDest, 0, 0, 0, &cpySource, nullptr);

			D3D12_RESOURCE_BARRIER cpyBarrier = {};
			cpyBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			cpyBarrier.Transition.pResource = mippedTexture.Get();
			cpyBarrier.Transition.Subresource = 0;
			cpyBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			cpyBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			commandList->ResourceBarrier(1, &cpyBarrier);

			auto uavHandle = uavHeap->GetCPUDescriptorHandleForHeapStart();
			// don't need to generate the first mip as it's just the texture we'll copy
			int mipsToGenerate = numMips;
			// can only process 4 mips per dispatch, break them into multiple passes if necessary
			while (mipsToGenerate > 0)
			{
				short passMips = std::min(mipsToGenerate, 4);

				// create UAVs for each desired mip level
				std::vector<D3D12_UNORDERED_ACCESS_VIEW_DESC> mipUavs;
				mipUavs.resize(numMips);
				for (uint8_t i = 0; i < passMips; ++i)
				{
					auto mipLevel = startingMip + i;
					auto& uav = mipUavs[i];
					uav.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
					uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					uav.Texture2D.MipSlice = mipLevel+1;
					mDevice->CreateUnorderedAccessView(mippedTexture.Get(), nullptr, &uav, uavHandle);
					uavHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}

				// create SRV for source texture
				D3D12_RESOURCE_DESC srcDesc = sourceTexture->GetDesc();
				D3D12_SHADER_RESOURCE_VIEW_DESC srcViewDesc = {};
				srcViewDesc.Format = srcDesc.Format;
				srcViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srcViewDesc.Texture2D.MipLevels = srcDesc.MipLevels;
				srcViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				mDevice->CreateShaderResourceView(sourceTexture.Get(), &srcViewDesc, uavHandle);
				uavHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				// fill CB
				D3D12_RANGE cbRange = { 0, 0 };
				MipConstantBuffer* cbPtr;
				mConstantBuffer->Map(0, &cbRange, reinterpret_cast<void**>(&cbPtr));
				*cbPtr = MipConstantBuffer{ 0, numMips, { 1.f / (srcDesc.Width >> (startingMip+1)), 1.f / (srcDesc.Height >> (startingMip+1)) }, 0 };
				mConstantBuffer->Unmap(0, nullptr);

				D3D12_CONSTANT_BUFFER_VIEW_DESC cb = {};
				cb.BufferLocation = mConstantBuffer->GetGPUVirtualAddress();
				cb.SizeInBytes = (sizeof(MipConstantBuffer)+ 255) & ~255;
				mDevice->CreateConstantBufferView(&cb, uavHandle);
				uavHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				// prepare for compute dispatch
				//auto gpuHandle = uavHeap->GetGPUDescriptorHandleForHeapStart();
				//gpuHandle.ptr += mDevice->Get
				commandList->SetPipelineState(mPipelineState.Get());
				commandList->SetComputeRootSignature(mRootSig.Get());
				ID3D12DescriptorHeap* mipHeaps[] = { uavHeap.Get() };
				commandList->SetDescriptorHeaps(_countof(mipHeaps), mipHeaps);
				commandList->SetComputeRootDescriptorTable(0, uavHeap->GetGPUDescriptorHandleForHeapStart());
				commandList->Dispatch((srcDesc.Width >> startingMip) / 8, (srcDesc.Height >> startingMip) / 8, 1);

				// get ready for the next pass
				mipsToGenerate -= 4;
				startingMip += 4;
			}

			commandList->Close();
			ID3D12CommandList* mipLists[] = { commandList.Get() };
			commandQueue->ExecuteCommandLists(1, mipLists);
			hr = hvk::render::WaitForGraphics(mDevice, commandQueue);
			assert(SUCCEEDED(hr));

			return hr;
		}
	}
}
