#pragma once

#include "CaptureManager.h"
#include <cstdint>
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

		const size_t kMaxCaptureAttempts = 10;

		bool LoadShaderByteCode(LPCWSTR filename, std::vector<uint8_t>& byteCodeOut)
		{
			DWORD codeSize;
			DWORD bytesRead;

			HANDLE shaderHandle = CreateFile2(filename, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
			codeSize = GetFileSize(shaderHandle, nullptr);
			byteCodeOut.resize(codeSize);
			bool readSuccess = ReadFile(shaderHandle, byteCodeOut.data(), codeSize, &bytesRead, nullptr);
			assert(readSuccess);
			assert(bytesRead == codeSize);

			return readSuccess && (bytesRead == codeSize);
		}

		HRESULT GetNextFrameResource(const CaptureManager& cm, IDXGIResource** outResource)
		{
			HRESULT hr = S_OK;

			hr = cm.AcquireFrameAsDXGIResource(outResource);
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
						hr = cm.AcquireFrameAsDXGIResource(outResource);
						if (SUCCEEDED(hr))
						{
							break;
						}
					}
				}
			}

			return hr;
		}

		HRESULT GenerateColorCorrectionLUT(
			ComPtr<ID3D12Device> device,
			ComPtr<ID3D12GraphicsCommandList> singleUse,
			ComPtr<ID3D12CommandQueue> commandQueue,
			ComPtr<ID3D12DescriptorHeap> uavHeap,
			ComPtr<ID3D12Resource> colorCorrectionTex)
		{
			HRESULT hr = S_OK;

			std::vector<uint8_t> lutByteCode;
			bool shaderLoadSuccess = LoadShaderByteCode(L"shaders\\lut_generator.cso", lutByteCode);
			assert(shaderLoadSuccess);

			D3D12_DESCRIPTOR_RANGE lutRange = {};
			lutRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			lutRange.NumDescriptors = 1;
			lutRange.BaseShaderRegister = 0;
			lutRange.RegisterSpace = 0;
			lutRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			
			D3D12_ROOT_PARAMETER lutParam = {};
			lutParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			lutParam.DescriptorTable.NumDescriptorRanges = 1;
			lutParam.DescriptorTable.pDescriptorRanges = &lutRange;
			lutParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			ComPtr<ID3D12RootSignature> rootSig;
			hr = hvk::render::CreateRootSignature(device, { lutParam }, {}, rootSig);
			assert(SUCCEEDED(hr));

			ComPtr<ID3D12PipelineState> lutPipelineState;
			hr = hvk::render::CreateComputePipelineState(device, rootSig, lutByteCode.data(), lutByteCode.size(), lutPipelineState);
			assert(SUCCEEDED(hr));

			D3D12_UNORDERED_ACCESS_VIEW_DESC lutDesc = {};
			lutDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			lutDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
			lutDesc.Texture3D.MipSlice = 0;
			lutDesc.Texture3D.WSize = 256;
			lutDesc.Texture3D.FirstWSlice = 0;
			auto uavHandle = uavHeap->GetCPUDescriptorHandleForHeapStart();
			device->CreateUnorderedAccessView(colorCorrectionTex.Get(), nullptr, &lutDesc, uavHandle);
			singleUse->SetPipelineState(lutPipelineState.Get());
			singleUse->SetComputeRootSignature(rootSig.Get());
			ID3D12DescriptorHeap* lutHeaps[] = { uavHeap.Get() };
			singleUse->SetDescriptorHeaps(_countof(lutHeaps), lutHeaps);
			singleUse->SetComputeRootDescriptorTable(0, uavHeap->GetGPUDescriptorHandleForHeapStart());
			singleUse->Dispatch(32, 32, 32);

			D3D12_RESOURCE_BARRIER lutBarrier = {};
			lutBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			lutBarrier.Transition.pResource = colorCorrectionTex.Get();
			lutBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			lutBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			lutBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			singleUse->ResourceBarrier(1, &lutBarrier);
			singleUse->Close();

			ID3D12CommandList* lutLists[] = { singleUse.Get() };
			commandQueue->ExecuteCommandLists(1, lutLists);
			hr = hvk::render::WaitForGraphics(device, commandQueue);

			return hr;
		}

		HRESULT GenerateMips(
			ComPtr<ID3D12Device> device,
			ComPtr<ID3D12GraphicsCommandList> singleUse,
			ComPtr<ID3D12CommandQueue> commandQueue,
			ComPtr<ID3D12DescriptorHeap> uavHeap,
			uint8_t numMips,
			ComPtr<ID3D12Resource> sourceTexture,
			ComPtr<ID3D12Resource> mippedTexture)
		{
			HRESULT hr = S_OK;

			std::vector<uint8_t> mipByteCode;
			bool shaderLoadSuccess = LoadShaderByteCode(L"shaders\\generate_mips.cso", mipByteCode);
			assert(shaderLoadSuccess);

			D3D12_DESCRIPTOR_RANGE mipUavRange = {};
			mipUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			mipUavRange.NumDescriptors = 4;
			mipUavRange.BaseShaderRegister = 0;
			mipUavRange.RegisterSpace = 0;
			mipUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_DESCRIPTOR_RANGE mipCbRange = {};
			mipCbRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			mipCbRange.NumDescriptors = 1;
			mipCbRange.BaseShaderRegister = 0;
			mipCbRange.RegisterSpace = 0;
			mipCbRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_DESCRIPTOR_RANGE mipSrvRange = {};
			mipSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			mipSrvRange.NumDescriptors = 1;
			mipSrvRange.BaseShaderRegister = 0;
			mipSrvRange.RegisterSpace = 0;
			mipSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_DESCRIPTOR_RANGE ranges[] = { mipUavRange, mipCbRange, mipSrvRange };

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

			ComPtr<ID3D12RootSignature> rootSig;
			hr = hvk::render::CreateRootSignature(device, { mipParam }, { bilinearSampler }, rootSig);
			assert(SUCCEEDED(hr));

			ComPtr<ID3D12PipelineState> mipPipelineState;
			hr = hvk::render::CreateComputePipelineState(device, rootSig, mipByteCode.data(), mipByteCode.size(), mipPipelineState);
			assert(SUCCEEDED(hr));

			// copy the source texture to mip 0 of the mipmap resource
			D3D12_RESOURCE_BARRIER preCpyBarrier = {};
			preCpyBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			preCpyBarrier.Transition.pResource = mippedTexture.Get();
			preCpyBarrier.Transition.Subresource = 0;
			preCpyBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			preCpyBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
			singleUse->ResourceBarrier(1, &preCpyBarrier);

			D3D12_RESOURCE_DESC srcDesc = sourceTexture->GetDesc();
			D3D12_TEXTURE_COPY_LOCATION cpySource = {};
			cpySource.pResource = sourceTexture.Get();
			cpySource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			uint64_t requiredSize = 0;
			device->GetCopyableFootprints(&srcDesc, 0, 1, 0, &cpySource.PlacedFootprint, nullptr, nullptr, &requiredSize);

			D3D12_TEXTURE_COPY_LOCATION cpyDest = {};
			cpyDest.pResource = mippedTexture.Get();
			cpyDest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			cpyDest.SubresourceIndex = 0;
			singleUse->CopyTextureRegion(&cpyDest, 0, 0, 0, &cpySource, nullptr);

			D3D12_RESOURCE_BARRIER cpyBarrier = {};
			cpyBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			cpyBarrier.Transition.pResource = mippedTexture.Get();
			cpyBarrier.Transition.Subresource = 0;
			cpyBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			cpyBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			singleUse->ResourceBarrier(1, &cpyBarrier);

			// create UAVs for each desired mip level
			auto uavHandle = uavHeap->GetCPUDescriptorHandleForHeapStart();
			std::vector<D3D12_UNORDERED_ACCESS_VIEW_DESC> mipUavs;
			mipUavs.resize(4);
			for (uint8_t i = 0; i < 4; ++i)
			{
				auto& uav = mipUavs[i];
				uav.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				uav.Texture2D.MipSlice = i+1;
				device->CreateUnorderedAccessView(mippedTexture.Get(), nullptr, &uav, uavHandle);
				uavHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}

			// create constant buffer for mip generation
			ComPtr<ID3D12Resource> constantBuffer;
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
			hr = device->CreateCommittedResource(&cbHeapProps, D3D12_HEAP_FLAG_NONE, &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constantBuffer));
			assert(SUCCEEDED(hr));

			// fill CB
			D3D12_RANGE cbRange = { 0, 0 };
			MipConstantBuffer* cbPtr;
			constantBuffer->Map(0, &cbRange, reinterpret_cast<void**>(&cbPtr));
			*cbPtr = MipConstantBuffer{ 0, numMips, {1.f / 1720.f, 1.f / 720.f}, 0 };
			constantBuffer->Unmap(0, nullptr);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cb = {};
			cb.BufferLocation = constantBuffer->GetGPUVirtualAddress();
			cb.SizeInBytes = (sizeof(MipConstantBuffer)+ 255) & ~255;
			device->CreateConstantBufferView(&cb, uavHandle);
			uavHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			// create SRV for source texture
			D3D12_SHADER_RESOURCE_VIEW_DESC srcViewDesc = {};
			srcViewDesc.Format = srcDesc.Format;
			srcViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srcViewDesc.Texture2D.MipLevels = srcDesc.MipLevels;
			srcViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			device->CreateShaderResourceView(sourceTexture.Get(), &srcViewDesc, uavHandle);
			uavHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			// prepare for compute dispatch
			singleUse->SetPipelineState(mipPipelineState.Get());
			singleUse->SetComputeRootSignature(rootSig.Get());
			ID3D12DescriptorHeap* mipHeaps[] = { uavHeap.Get() };
			singleUse->SetDescriptorHeaps(_countof(mipHeaps), mipHeaps);
			singleUse->SetComputeRootDescriptorTable(0, uavHeap->GetGPUDescriptorHandleForHeapStart());
			singleUse->Dispatch(3440 / 8, 1440 / 8, 1);

			// transition mips for SRV
			D3D12_RESOURCE_BARRIER mipBarrier = {};
			mipBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			mipBarrier.Transition.pResource = mippedTexture.Get();
			mipBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			mipBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			mipBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			singleUse->ResourceBarrier(1, &mipBarrier);

			singleUse->Close();
			ID3D12CommandList* mipLists[] = { singleUse.Get() };
			commandQueue->ExecuteCommandLists(1, mipLists);
			hr = hvk::render::WaitForGraphics(device, commandQueue);
			assert(SUCCEEDED(hr));

			return hr;
		}


		HRESULT GenerateDummyTexture(
			ComPtr<ID3D12Device> device, 
			ComPtr<ID3D12GraphicsCommandList> singleUseCommandList, 
			ComPtr<ID3D12CommandQueue> commandQueue, 
			ComPtr<ID3D12Resource>& d3d12Resource)
		{
			HRESULT hr = S_OK;

			D3D12_HEAP_PROPERTIES texHeap = {};
			texHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
			texHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			texHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			texHeap.CreationNodeMask = 1;
			texHeap.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC resourceDesc = {};
			resourceDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			resourceDesc.MipLevels = 1;
			resourceDesc.Width = 3440;
			resourceDesc.Height = 1440;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			device->CreateCommittedResource(
				&texHeap, 
				D3D12_HEAP_FLAG_NONE, 
				&resourceDesc, 
				D3D12_RESOURCE_STATE_COPY_DEST, 
				nullptr, 
				IID_PPV_ARGS(&d3d12Resource));

			ComPtr<ID3D12Resource> tempBuffer;

			D3D12_HEAP_PROPERTIES heapProps = {};
			heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProps.CreationNodeMask = 1;
			heapProps.VisibleNodeMask = 1;

			uint32_t paddedWidth = Align(3440 * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
			D3D12_RESOURCE_DESC bufferDesc = {};
			bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			bufferDesc.Alignment = 0;
			bufferDesc.Width = paddedWidth * 1440;
			bufferDesc.Height = 1;
			bufferDesc.DepthOrArraySize = 1;
			bufferDesc.MipLevels = 1;
			bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
			bufferDesc.SampleDesc.Count = 1;
			bufferDesc.SampleDesc.Quality = 0;
			bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			hr = device->CreateCommittedResource(
				&heapProps, 
				D3D12_HEAP_FLAG_NONE, 
				&bufferDesc, 
				D3D12_RESOURCE_STATE_GENERIC_READ, 
				nullptr, 
				IID_PPV_ARGS(&tempBuffer));
			assert(SUCCEEDED(hr));

			D3D12_RANGE readRange = {};
			readRange.Begin = 0;
			readRange.End = 0;

			uint8_t* texBegin;
			tempBuffer->Map(0, &readRange, reinterpret_cast<void**>(&texBegin));
			for (size_t y = 0; y < 1440; ++y)
			{
				size_t scanlineOffset = y * paddedWidth;
				for (size_t x = paddedWidth-(3440*4); x < paddedWidth; x+=4)
				{
					uint8_t* thisPixel = texBegin + scanlineOffset + x;
					*thisPixel = 255;
					*(thisPixel + 1) = 0;
					*(thisPixel + 2) = 0;
					*(thisPixel + 3) = 255;
				}
			}
			tempBuffer->Unmap(0, nullptr);

			D3D12_TEXTURE_COPY_LOCATION cpySource = {};
			cpySource.pResource = tempBuffer.Get();
			cpySource.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			//cpySource.PlacedFootprint.Offset = 0;
			//cpySource.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			//cpySource.PlacedFootprint.Footprint.Width = 3440;
			//cpySource.PlacedFootprint.Footprint.Height = 1440;
			//cpySource.PlacedFootprint.Footprint.Depth = 1;
			//cpySource.PlacedFootprint.Footprint.RowPitch = 3440 * 4;
			//cpySource.PlacedFootprint.Footprint.RowPitch = Align(3440 * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

			uint64_t requiredSize = 0;
			device->GetCopyableFootprints(&resourceDesc, 0, 1, 0, &cpySource.PlacedFootprint, nullptr, nullptr, &requiredSize);

			D3D12_TEXTURE_COPY_LOCATION cpyDest = {};
			cpyDest.pResource = d3d12Resource.Get();
			cpyDest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			cpyDest.SubresourceIndex = 0;

			singleUseCommandList->CopyTextureRegion(&cpyDest, 0, 0, 0, &cpySource, nullptr);

			D3D12_RESOURCE_BARRIER transBarrier = {};
			transBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			transBarrier.Transition.pResource = d3d12Resource.Get();
			transBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			transBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			transBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			singleUseCommandList->ResourceBarrier(1, &transBarrier);

			singleUseCommandList->Close();

			ID3D12CommandList* copyList[] = { singleUseCommandList.Get() };
			commandQueue->ExecuteCommandLists(1, copyList);

			hr = render::WaitForGraphics(device, commandQueue);

			return hr;
		}
	}
}
