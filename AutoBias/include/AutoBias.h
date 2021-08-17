#pragma once

#include <wrl/client.h>
#include <d3d12.h>
#include <cstdint>
#include <memory>
#include <vector>

#include <ShaderService.h>

#include "generator/LEDGenerator.h"
#include "generator/LuminanceGenerator.h"
#include "generator/MipGenerator.h"

using namespace Microsoft::WRL;

namespace hvk {
	namespace control {
		//class ArduinoController;
		class DeviceController;
		class ArduinoDevice;
		struct Color;
	}

}

namespace hvk
{
	struct Color;

	namespace bias
	{
		using Device = ComPtr<ID3D12Device>;
		using Surface = ComPtr<ID3D12Resource>;
		using LinearBuffer = ComPtr<ID3D12Resource>;
		using CommandList = ComPtr<ID3D12GraphicsCommandList4>;
		using CommandAllocator = ComPtr<ID3D12CommandAllocator>;
		using CommandQueue = ComPtr<ID3D12CommandQueue>;

		class AutoBias
		{
		public:
			AutoBias(
				Device device, 
				uint32_t gridWidth, 
				uint32_t gridHeight, 
				std::vector<int> ledMask,
				uint32_t surfaceWidth,
				uint32_t surfaceHeight);
			~AutoBias();

			HRESULT Update(Surface surface);

			const LinearBuffer GetLEDBuffer() const;

		private:
			Device mDevice;
			Surface mMipmappedSurface;
			Surface mLuminanceSurface;
			Surface mColorCorrectionSurface;
			LinearBuffer mLEDBuffer;
			LinearBuffer mLEDCopyBuffer;

			d3d12::LEDGenerator mLEDGenerator;
			d3d12::LuminanceGenerator mLuminanceGenerator;
			d3d12::MipGenerator mMipGenerator;

			uint32_t mGridWidth;
			uint32_t mGridHeight;
			uint16_t mNumMips;
			std::vector<int> mLEDMask;

			std::vector<hvk::control::Color> mLEDWriteBuffer;
			//std::unique_ptr<control::ArduinoController> mArduinoController;
			//std::unique_ptr<control::DeviceController<control::ArduinoDevice>> mArduinoDevice;
			std::unique_ptr<control::DeviceController> mBiasDevice;

			CommandQueue mCommandQueue;
			CommandAllocator mCommandAllocator;
			CommandList mCommandList;

			static std::shared_ptr<hvk::render::shader::ShaderService> sShaderService;
		};
	}
}
