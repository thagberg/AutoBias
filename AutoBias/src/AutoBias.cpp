#define NOMINMAX 1

#include "AutoBias.h"

#include <cmath>
//#include <wrl/implements.h>

#include <Render.h>
#include <ArduinoControl/ArduinoControl.h>

namespace hvk
{
	namespace bias
	{
		// Bucket Dimension is the number of pixels processed in each XY dimension
		// for calculating LED colors. 
		const float kBucketDimension = 8.f;

		AutoBias::AutoBias(
			Device device, 
			uint32_t gridWidth, 
			uint32_t gridHeight, 
			std::vector<int> ledMask,
			uint32_t surfaceWidth,
			uint32_t surfaceHeight)
			: mDevice(device)
			, mMipmappedSurface(nullptr)
			, mLuminanceSurface(nullptr)
			, mColorCorrectionSurface(nullptr)
			, mLEDBuffer(nullptr)
			, mLEDCopyBuffer(nullptr)
			, mLEDGenerator(mDevice)
			, mLuminanceGenerator(mDevice)
			, mMipGenerator(mDevice)
			, mGridWidth(gridWidth)
			, mGridHeight(gridHeight)
			, mNumMips(0)
			, mLEDMask(ledMask)
			, mLEDWriteBuffer()
			, mArduinoController(nullptr)
			, mCommandQueue(nullptr)
			, mCommandAllocator(nullptr)
			, mCommandList(nullptr)
		{
			HRESULT hr = S_OK;

			// Create mipped surface
			{
				const float maxX = mGridWidth * kBucketDimension;
				const float maxY = mGridHeight * kBucketDimension;

				// Find the number of mipmaps required from the surface resolution until
				// we can process LED colors in a single pass
				const auto xMips = static_cast<uint16_t>(std::ceil(std::log2(surfaceWidth / maxX)));
				const auto yMips = static_cast<uint16_t>(std::ceil(std::log2(surfaceHeight / maxY)));
				mNumMips = std::min(xMips, yMips);
				// We'll calculate luminance on a further mip than is required
				++mNumMips;

				hr = render::CreateResource(
					mDevice, 
					D3D12_RESOURCE_DIMENSION_TEXTURE2D, 
					DXGI_FORMAT_B8G8R8A8_UNORM, 
					surfaceWidth, 
					surfaceHeight, 
					1, 
					mNumMips, 
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 
					D3D12_TEXTURE_LAYOUT_UNKNOWN,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_HEAP_TYPE_DEFAULT,
					mMipmappedSurface);
				assert(SUCCEEDED(hr));
			}

			// Create luminance surface
			{
				hr = render::CreateResource(
					mDevice,
					D3D12_RESOURCE_DIMENSION_TEXTURE2D,
					DXGI_FORMAT_A8_UNORM,
					surfaceWidth >> (mNumMips - 1),
					surfaceHeight >> (mNumMips - 1),
					1,
					1,
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
					D3D12_TEXTURE_LAYOUT_UNKNOWN,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_HEAP_TYPE_DEFAULT,
					mLuminanceSurface);
				assert(SUCCEEDED(hr));
			}

			// Create LED buffer
			{
				hr = render::CreateResource(
					mDevice,
					D3D12_RESOURCE_DIMENSION_BUFFER,
					DXGI_FORMAT_UNKNOWN,
					mGridWidth * mGridHeight * 4,
					1,
					1,
					1,
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
					D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_HEAP_TYPE_DEFAULT,
					mLEDBuffer);
				assert(SUCCEEDED(hr));

				// We can't create a readback buffer with unordered-access.
				// Thus, we will draw to the first buffer and copy it to
				// the readback buffer each frame
				hr = render::CreateResource(
					mDevice,
					D3D12_RESOURCE_DIMENSION_BUFFER,
					DXGI_FORMAT_UNKNOWN,
					mGridWidth * mGridHeight * 4,
					1,
					1,
					1,
					D3D12_RESOURCE_FLAG_NONE,
					D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_HEAP_TYPE_READBACK,
					mLEDCopyBuffer);
				assert(SUCCEEDED(hr));
			}

			// Create ArduinoController
			{
				// calculate the number of active LEDs from the mask
				size_t numLeds = 0;
				for (const auto m : mLEDMask)
				{
					if (m >= 0)
					{
						++numLeds;
					}
				}
				mArduinoController = std::make_unique<control::ArduinoController>(numLeds);
				mLEDWriteBuffer.resize(numLeds);
			}

			// Create command list
			{
				hr = render::CreateCommandQueue(device, mCommandQueue);
				assert(SUCCEEDED(hr));
				hr = render::CreateCommandAllocator(device, mCommandAllocator);
				assert(SUCCEEDED(hr));
				hr = render::CreateCommandList(device, mCommandAllocator, nullptr, mCommandList);
				assert(SUCCEEDED(hr));
				mCommandList->Close();
			}

			// Generate color correction tex
			{
				hr = render::CreateResource(
					mDevice,
					D3D12_RESOURCE_DIMENSION_TEXTURE3D,
					DXGI_FORMAT_B8G8R8A8_UNORM,
					256,
					256,
					256,
					1,
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
					D3D12_TEXTURE_LAYOUT_UNKNOWN,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_HEAP_TYPE_DEFAULT,
					mColorCorrectionSurface);
				assert(SUCCEEDED(hr));

				auto heapProps = render::HeapPropertiesDefault();

				mCommandList->Reset(mCommandAllocator.Get(), nullptr);
				hr = bias::GenerateColorCorrectionLUT(mDevice, mCommandList, mCommandQueue, mColorCorrectionSurface);
				assert(SUCCEEDED(hr));
			}

			// Finally, initialize the Arduino controller
			int initStatus = mArduinoController->Init();
			assert(initStatus == 0);
		}

		AutoBias::~AutoBias()
		{
			mArduinoController->Stop();
			mArduinoController.reset(nullptr);
		}
		
		HRESULT AutoBias::Update(Surface surface)
		{
			HRESULT hr = S_OK;

			hr = mCommandAllocator->Reset();
			assert(SUCCEEDED(hr));

			// generate mipmaps
			mCommandList->Reset(mCommandAllocator.Get(), nullptr);
			hr = mMipGenerator.Generate(mCommandList, mCommandQueue, mNumMips - 1, 0, surface, mMipmappedSurface);
			assert(SUCCEEDED(hr));

			// generate luminance
			mCommandList->Reset(mCommandAllocator.Get(), nullptr);
			hr = mLuminanceGenerator.Generate(mCommandList, mCommandQueue, mMipmappedSurface, mNumMips - 1, mLuminanceSurface);
			assert(SUCCEEDED(hr));

			// generate LED colors
			mCommandList->Reset(mCommandAllocator.Get(), nullptr);
			hr = mLEDGenerator.Generate(
				mCommandList, 
				mCommandQueue, 
				mMipmappedSurface, 
				mNumMips - 1, 
				mGridWidth, 
				mGridHeight, 
				mLEDBuffer, 
				mLEDCopyBuffer,
				mColorCorrectionSurface);
			assert(SUCCEEDED(hr));

			// write LEDs out to Arduino
			{
				const size_t ledSize = mGridHeight * mGridWidth * 4;
				const D3D12_RANGE ledRange = { 0, ledSize };
				uint8_t* ledPtr;

				// Iterate over values in the buffer we wrote out to in the LED generator.
				// For each one we'll compare it to its respective value in the LED mask
				// and use that to determine if it's an active LED and where it's place
				// in the write-out buffer is.
				mLEDCopyBuffer->Map(0, &ledRange, reinterpret_cast<void**>(&ledPtr));
				{
					size_t bufferIndex = 0;
					for (size_t bufferIndex = 0; bufferIndex < ledSize; bufferIndex += 4)
					{
						auto ledIndex = mLEDMask[bufferIndex / 4];
						if (ledIndex >= 0)
						{
							hvk::Color c = {
								ledPtr[bufferIndex],
								ledPtr[bufferIndex + 1],
								ledPtr[bufferIndex + 2]
							};
							mLEDWriteBuffer[ledIndex] = c;
						}
					}
				}
				mLEDCopyBuffer->Unmap(0, nullptr);

				mArduinoController->WritePixels(mLEDWriteBuffer);
			}

			return hr;
		}

		const LinearBuffer AutoBias::GetLEDBuffer() const
		{
			return mLEDCopyBuffer;
		}
	}
}