#pragma once

#include <Windows.h>
#include <fileapi.h>

#include <stdint.h>
#include <vector>
#include <array>
#include <thread>
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

	template <uint8_t Width, uint8_t Height>
	struct Framebuffer
	{
		std::array<Color, Width * Height> buffer;
	};

	const uint8_t kFramebuffers = 2;

	namespace control
	{
		template <uint8_t Width, uint8_t Height>
		class ArduinoController
		{
		public:
			ArduinoController();
			~ArduinoController();
			int8_t Init();
			DWORD WritePixels(const std::array<Color, Width*Height>& pixels);
		private:
			HANDLE mCommHandle;
			DCB mDcb;
			bool mDeviceReady;
			std::array<Framebuffer<Width, Height>, kFramebuffers> mFramebuffers;
			uint8_t mBackbuffer;
			bool mDeviceConnected;
			std::thread mIOThread;
			std::shared_mutex mBufferMutex;
			bool mDisplayReady;
			char mReadBuffer[256];

			void UpdateDevice();
		};

		template <uint8_t W, uint8_t H>
		ArduinoController<W, H>::ArduinoController()
			: mCommHandle(INVALID_HANDLE_VALUE)
			, mDcb()
			, mDeviceReady(false)
			, mFramebuffers()
			, mBackbuffer(0)
			, mDeviceConnected(false)
			, mIOThread()
			, mBufferMutex()
			, mDisplayReady(false)
		{
			ZeroMemory(mReadBuffer, 256);

			// initialize framebuffers
			for (auto& fb : mFramebuffers)
			{
				fb.buffer.fill({ 255, 255, 255 });
			}
		}

		template <uint8_t W, uint8_t H>
		ArduinoController<W, H>::~ArduinoController()
		{
			mDeviceReady = false;
			mDeviceConnected = false;

			auto success = CancelIo(mCommHandle);
			assert(success);
			mIOThread.join();
			success = CloseHandle(mCommHandle);
			assert(success);
		}

		template <uint8_t W, uint8_t H>
		int8_t ArduinoController<W, H>::Init()
		{
			int8_t ret = 0;

			mCommHandle = CreateFileA(
				"COM4",
				GENERIC_READ | GENERIC_WRITE,
				NULL,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
			assert(mCommHandle != INVALID_HANDLE_VALUE);
			if (mCommHandle == INVALID_HANDLE_VALUE)
			{
				return -1;
			}

			memset(&mDcb, 0, sizeof(DCB));
			bool success = GetCommState(mCommHandle, &mDcb);
			assert(success);
			if (!success)
			{
				return -2;
			}

			mDcb.BaudRate = CBR_38400;
			mDcb.ByteSize = 8;
			mDcb.Parity = NOPARITY;
			mDcb.StopBits = ONESTOPBIT;
			mDcb.fRtsControl = RTS_CONTROL_ENABLE;
			mDcb.fDtrControl = DTR_CONTROL_ENABLE;
			success = SetCommState(mCommHandle, &mDcb);
			assert(success);
			if (!success)
			{
				return -3;
			}

			success = GetCommState(mCommHandle, &mDcb);
			assert(success);
			if (!success)
			{
				return -4;
			}

			COMMTIMEOUTS timeouts = {};
			timeouts.ReadTotalTimeoutConstant = 1000;
			timeouts.ReadIntervalTimeout = 0;
			timeouts.ReadTotalTimeoutMultiplier = 1;
			timeouts.WriteTotalTimeoutConstant = 1000;
			timeouts.WriteTotalTimeoutMultiplier = 1;
			success = SetCommTimeouts(mCommHandle, &timeouts);
			assert(success);
			if (!success)
			{
				return -5;
			}

			mDeviceConnected = true;
			mDeviceReady = true;

			mIOThread = std::thread(&ArduinoController::UpdateDevice, this);

			return ret;
		}

		template <uint8_t W, uint8_t H>
		void ArduinoController<W, H>::UpdateDevice()
		{
			while (mDeviceConnected)
			{
				if (mDisplayReady)
				{
					mDisplayReady = false;
					int frontbufferIndex;
					{
						std::unique_lock swapLock(mBufferMutex);
						mBackbuffer++;
						if (mBackbuffer > 1)
						{
							mBackbuffer = 0;
						}

						frontbufferIndex = ~mBackbuffer & 1;
					}

					const auto& frontbuffer = mFramebuffers[frontbufferIndex];

					const uint8_t numPixelsToWrite = W * H;
					DWORD numBytesWritten;
					bool writeSuccess = WriteFile(mCommHandle, frontbuffer.buffer.data(), sizeof(Color) * numPixelsToWrite, &numBytesWritten, nullptr);
					assert(writeSuccess);

					DWORD numBytesRead;
					bool readSuccess = ReadFile(mCommHandle, mReadBuffer, 1, &numBytesRead, nullptr);
					assert(readSuccess);
				}

				Sleep(16);
			}
		}

		template <uint8_t W, uint8_t H>
		DWORD ArduinoController<W, H>::WritePixels(const std::array<Color, W*H>& colors)
		{
			{
				std::unique_lock bufferLock(mBufferMutex);
				auto& backbuffer = mFramebuffers[mBackbuffer];
				auto copyStatus = memcpy_s(backbuffer.buffer.data(), sizeof(Color) * W * H, colors.data(), sizeof(Color) * W * H);
				assert(copyStatus == S_OK);
				mDisplayReady = true;
			}

			return 0;
		}
	}
}
