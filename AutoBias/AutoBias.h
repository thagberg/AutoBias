#pragma once

#include <wrl/client.h>
#include <d3d12.h>
#include <cstdint>
#include <span>
#include <memory>
#include <vector>

#include <LEDGenerator.h>
#include <LuminanceGenerator.h>
#include <MipGenerator.h>

using namespace Microsoft::WRL;

namespace hvk
{
	namespace control
	{
		class ArduinoController;
	}

	struct Color;

	namespace bias
	{
		using Device = ComPtr<ID3D12Device>;
		using Surface = ComPtr<ID3D12Resource>;
		using LinearBuffer = ComPtr<ID3D12Resource>;
		using CommandList = ComPtr<ID3D12GraphicsCommandList>;
		using CommandAllocator = ComPtr<ID3D12CommandAllocator>;
		using CommandQueue = ComPtr<ID3D12CommandQueue>;

		class AutoBias
		{
		public:
			AutoBias(
				Device device, 
				uint32_t gridWidth, 
				uint32_t gridHeight, 
				std::span<int> ledMask,
				uint32_t surfaceWidth,
				uint32_t surfaceHeight);

			HRESULT Update(Surface surface);

		private:
			Device mDevice;
			Surface mMipmappedSurface;
			Surface mLuminanceSurface;
			Surface mColorCorrectionSurface;
			LinearBuffer mLEDBuffer;

			d3d12::LEDGenerator mLEDGenerator;
			d3d12::LuminanceGenerator mLuminanceGenerator;
			d3d12::MipGenerator mMipGenerator;

			uint32_t mGridWidth;
			uint32_t mGridHeight;
			uint16_t mNumMips;
			std::span<int> mLEDMask;

			std::vector<Color> mLEDWriteBuffer;
			std::unique_ptr<control::ArduinoController> mArduinoController;

			CommandQueue mCommandQueue;
			CommandAllocator mCommandAllocator;
			CommandList mCommandList;
		};
	}
}
