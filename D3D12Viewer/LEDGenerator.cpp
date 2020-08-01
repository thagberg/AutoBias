#include "LEDGenerator.h"

namespace hvk
{
	namespace d3d12
	{
		#pragma pack(16)
		struct LEDConstantBuffer
		{
			uint32_t DispatchSize[3];
			float TexelSize[2];
		};

		LEDGenerator::LEDGenerator(ComPtr<ID3D12Device> device)
			: mDevice(device)
		{
			assert(mDevice != nullptr);

			std::vector<uint8_t> ledByteCode;
			bool shaderLoadSuccess = bias::LoadShaderByteCode(L"shaders\\led_calc.cso", ledByteCode);
			assert(shaderLoadSuccess);

			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.NumDescriptors = 4;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			HRESULT hr = mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDescriptorHeap));
			assert(SUCCEEDED(hr));

			D3D12_DESCRIPTOR_RANGE srvRange = {};
			srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			srvRange.NumDescriptors = 2;
			srvRange.BaseShaderRegister = 0;
			srvRange.RegisterSpace = 0;
			srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_DESCRIPTOR_RANGE uavRange = {};
			uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			uavRange.NumDescriptors = 1;
			uavRange.BaseShaderRegister = 0;
			uavRange.RegisterSpace = 0;
			uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_DESCRIPTOR_RANGE cbvRange = {};
			cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			cbvRange.NumDescriptors = 1;
			cbvRange.BaseShaderRegister = 0;
			cbvRange.RegisterSpace = 0;
			cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_DESCRIPTOR_RANGE ranges[] = { srvRange, uavRange, cbvRange };
			
			D3D12_ROOT_PARAMETER param = {};
			param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param.DescriptorTable.NumDescriptorRanges = _countof(ranges);
			param.DescriptorTable.pDescriptorRanges = ranges;
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

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

			hr = render::CreateRootSignature(mDevice, { param }, { bilinearSampler }, mRootSig);
			assert(SUCCEEDED(hr));

			hr = render::CreateComputePipelineState(mDevice, mRootSig, ledByteCode.data(), ledByteCode.size(), mPipelineState);
			assert(SUCCEEDED(hr));

			D3D12_RESOURCE_DESC cbDesc = {};
			cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			cbDesc.Format = DXGI_FORMAT_UNKNOWN;
			cbDesc.Alignment = 0;
			cbDesc.Width = (sizeof(LEDConstantBuffer) + 255) & ~255;
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

		HRESULT LEDGenerator::Generate(
			ComPtr<ID3D12GraphicsCommandList> commandList,
			ComPtr<ID3D12CommandQueue> commandQueue,
			ComPtr<ID3D12Resource> sourceTexture,
			uint32_t sourceMip,
			uint32_t ledWidth,
			uint32_t ledHeight,
			ComPtr<ID3D12Resource> ledTexture,
			ComPtr<ID3D12Resource> colorCorrectionTexture,
			ComPtr<ID3D12Resource> ledCopyBuffer)
		{
			HRESULT hr = S_OK;
			auto heapHandle = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			auto gpuHandle = mDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

			D3D12_RESOURCE_DESC srcDesc = sourceTexture->GetDesc();
			D3D12_RESOURCE_DESC corDesc = colorCorrectionTexture->GetDesc();
			D3D12_RESOURCE_DESC ledDesc = ledTexture->GetDesc();

			// create SRV for source texture
			D3D12_SHADER_RESOURCE_VIEW_DESC srcViewDesc = {};
			srcViewDesc.Format = srcDesc.Format;
			srcViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srcViewDesc.Texture2D.MipLevels = 1;
			srcViewDesc.Texture2D.MostDetailedMip = sourceMip;
			srcViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			mDevice->CreateShaderResourceView(sourceTexture.Get(), &srcViewDesc, heapHandle);
			heapHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			// create SRV for color correction texture
			D3D12_SHADER_RESOURCE_VIEW_DESC corViewDesc = {};
			corViewDesc.Format = corDesc.Format;
			corViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			corViewDesc.Texture3D.MipLevels = 1;
			corViewDesc.Texture3D.MostDetailedMip = 0;
			corViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			mDevice->CreateShaderResourceView(colorCorrectionTexture.Get(), &corViewDesc, heapHandle);
			heapHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			// create UAV for LED texture
			D3D12_UNORDERED_ACCESS_VIEW_DESC ledUav = {};
			ledUav.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			//ledUav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			//ledUav.Format = DXGI_FORMAT_UNKNOWN;
			ledUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			//ledUav.Texture2D.MipSlice = 0;
			ledUav.Buffer.FirstElement = 0;
			ledUav.Buffer.NumElements = ledDesc.Width / 4;
			//ledUav.Buffer.StructureByteStride = 4;
			mDevice->CreateUnorderedAccessView(ledTexture.Get(), nullptr, &ledUav, heapHandle);
			heapHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			// fill CB
			D3D12_RANGE cbRange = { 0, 0 };
			LEDConstantBuffer* cbPtr;
			hr = mConstantBuffer->Map(0, &cbRange, reinterpret_cast<void**>(&cbPtr));
			assert(SUCCEEDED(hr));
			//auto texelWidth = srcDesc.Width >> sourceMip;
			//auto texelHeight = srcDesc.Height >> sourceMip;
			auto texelWidth = ledWidth * 8;
			auto texelHeight = ledHeight * 8;
			*cbPtr = LEDConstantBuffer{
				{ledWidth, ledHeight, 1},
				{ 1.f / texelWidth, 1.f / texelHeight }
			};
			mConstantBuffer->Unmap(0, nullptr);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbView = {};
			cbView.BufferLocation = mConstantBuffer->GetGPUVirtualAddress();
			cbView.SizeInBytes = (sizeof(LEDConstantBuffer) + 255) & ~255;
			mDevice->CreateConstantBufferView(&cbView, heapHandle);
			heapHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			// prepare for compute dispatch
			commandList->SetPipelineState(mPipelineState.Get());
			commandList->SetComputeRootSignature(mRootSig.Get());
			ID3D12DescriptorHeap* ledHeaps[] = { mDescriptorHeap.Get() };
			commandList->SetDescriptorHeaps(_countof(ledHeaps), ledHeaps);
			commandList->SetComputeRootDescriptorTable(0, gpuHandle);
			commandList->Dispatch(ledWidth, ledHeight, 1);

			// transition LED texture from UAV -> Copy Source
			D3D12_RESOURCE_BARRIER cpyBarrier = {};
			cpyBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			cpyBarrier.Transition.pResource = ledTexture.Get();
			cpyBarrier.Transition.Subresource = 0;
			cpyBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			cpyBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
			commandList->ResourceBarrier(1, &cpyBarrier);

			// copy LED texture to buffer
			//commandList->CopyResource(ledCopyBuffer.Get(), ledTexture.Get());
			D3D12_TEXTURE_COPY_LOCATION cpySource = {};
			cpySource.pResource = ledTexture.Get();
			//cpySource.Type = d3d12_buffer_co;
			uint64_t requiredSize = 0;
			mDevice->GetCopyableFootprints(&ledDesc, 0, 1, 0, &cpySource.PlacedFootprint, nullptr, nullptr, &requiredSize);

			auto cpyDesc = ledCopyBuffer->GetDesc();
			D3D12_TEXTURE_COPY_LOCATION cpyDest = {};
			cpyDest.pResource = ledCopyBuffer.Get();
			cpyDest.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			uint64_t copySize = 0;
			mDevice->GetCopyableFootprints(&cpyDesc, 0, 1, 0, &cpyDest.PlacedFootprint, nullptr, nullptr, &copySize);

			//commandList->CopyTextureRegion(&cpyDest, 0, 0, 0, &cpySource, nullptr);
			commandList->CopyResource(ledCopyBuffer.Get(), ledTexture.Get());

			// transition LED texture back to UAV
			D3D12_RESOURCE_BARRIER uavBarrier = {};
			uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			uavBarrier.Transition.pResource = ledTexture.Get();
			uavBarrier.Transition.Subresource = 0;
			uavBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
			uavBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			commandList->ResourceBarrier(1, &uavBarrier);

			commandList->Close();
			ID3D12CommandList* ledLists[] = { commandList.Get() };
			commandQueue->ExecuteCommandLists(_countof(ledLists), ledLists);
			hr = hvk::render::WaitForGraphics(mDevice, commandQueue);
			assert(SUCCEEDED(hr));

			return hr;
		}
	}
}
