#pragma once

#include <Windows.h>
#include <fileapi.h>

#include <stdint.h>
#include <thread>
#include <vector>
#include <array>
#include <cassert>
#include <future>
#include <mutex>
#include <shared_mutex>


namespace hvk
{
	struct Color
	{
		uint8_t r;
		uint8_t g;
		uint8_t b;
	};

	struct Framebuffer
	{
		std::vector<Color> buffer;
	};

	const uint8_t kFramebuffers = 2;

	const Color kWhite = { 255, 255, 255 };

	namespace control
	{
		class ArduinoController
		{
		public:
			ArduinoController(size_t size);
			~ArduinoController();
			int8_t Init();
			bool Stop();
			DWORD WritePixels(const std::vector<Color>& pixels);
		private:
			HANDLE mCommHandle;
			DCB mDcb;
			bool mDeviceReady;
			std::array<Framebuffer, kFramebuffers> mFramebuffers;
			uint8_t mBackbuffer;
			bool mDeviceConnected;
			std::thread mIOThread;
			std::shared_mutex mBufferMutex;
			bool mDisplayReady;
			char mReadBuffer[256];
			size_t mSize;

			void UpdateDevice();
		};

	}
}
