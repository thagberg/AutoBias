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
			bool mWaitingOnRead;
			OVERLAPPED mOverlappedRead;

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
			, mWaitingOnRead(false)
			, mOverlappedRead{0}
		{
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

			CancelIo(mCommHandle);
			mIOThread.join();
			CloseHandle(mCommHandle);
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
				FILE_FLAG_OVERLAPPED,
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

			mDcb.BaudRate = CBR_9600;
			mDcb.ByteSize = 8;
			mDcb.Parity = NOPARITY;
			mDcb.StopBits = ONESTOPBIT;
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
				if (mWaitingOnRead)
				{
					//GetOverlappedResult(mCommHandle)
					DWORD numBytesRead;
					bool readSuccess = GetOverlappedResult(mCommHandle, &mOverlappedRead, &numBytesRead, true);
					if (readSuccess)
					{ 
						ZeroMemory(&mOverlappedRead, sizeof(OVERLAPPED));
						mWaitingOnRead = false;
						mDeviceReady = true;
					}
					else
					{
						DWORD lastErr = GetLastError();
						assert(lastErr == ERROR_IO_PENDING || lastErr == ERROR_IO_INCOMPLETE);
					}
				}
				else
				{
					char readBuffer[256] = { 0 };
					DWORD numBytesRead;
					//mOverlappedRead.hEvent = CreateEvent(nullptr, true, false, nullptr);
					bool readSuccess = ReadFile(mCommHandle, readBuffer, 256, &numBytesRead, &mOverlappedRead);
					//assert(readSuccess);
					if (!readSuccess)
					{
						assert(GetLastError() == ERROR_IO_PENDING);
						mWaitingOnRead = true;
					}
					else
					{
						mDeviceReady = true;
					}
				}

				if (mDeviceReady)
				{
					mDeviceReady = false;

					// first swap the buffers
					{
						std::unique_lock swapLock(mBufferMutex);
						mBackbuffer = mBackbuffer % 1;
					}

					// then update the device
					const uint8_t numPixelsToWrite = W * H;
					bool writeSuccess = false;
					{
						std::shared_lock bufferLock(mBufferMutex);
						const auto& frontbuffer = mFramebuffers[mBackbuffer % 1];
						DWORD numBytesWritten;
						OVERLAPPED overlappedWrite = { 0 };
						writeSuccess = WriteFile(mCommHandle, frontbuffer.buffer.data(), sizeof(Color) * numPixelsToWrite, &numBytesWritten, &overlappedWrite);
					}
					if (!writeSuccess)
					{
						DWORD lastError = GetLastError();
						assert(lastError == ERROR_IO_PENDING);
					}
					//assert(writeSuccess = true);

					// then confirm the device response
					//char readBuffer[256] = { 0 };
					//DWORD numBytesRead;
					//bool readSuccess = ReadFile(mCommHandle, readBuffer, 1, &numBytesRead, nullptr);
					//assert(readSuccess = true);

					//mDeviceReady = true;
				}

				Sleep(16);
			}
		}

		template <uint8_t W, uint8_t H>
		DWORD ArduinoController<W, H>::WritePixels(const std::array<Color, W*H>& colors)
		{
			{
				std::shared_lock bufferLock(mBufferMutex);
				auto& backbuffer = mFramebuffers[mBackbuffer];
				auto copyStatus = memcpy_s(backbuffer.buffer.data(), sizeof(Color) * W * H, colors.data(), sizeof(Color) * W * H);
				assert(copyStatus == S_OK);
			}

			return 0;
		}
	}
}
