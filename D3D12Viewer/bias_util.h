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
		const size_t kMaxCaptureAttempts = 10;

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
			cpySource.PlacedFootprint.Offset = 0;
			cpySource.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			cpySource.PlacedFootprint.Footprint.Width = 3440;
			cpySource.PlacedFootprint.Footprint.Height = 1440;
			cpySource.PlacedFootprint.Footprint.Depth = 1;
			cpySource.PlacedFootprint.Footprint.RowPitch = 3440 * 4;
			cpySource.PlacedFootprint.Footprint.RowPitch = Align(3440 * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

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
