#include "pch.h"
#include "CaptureManager.h"

#pragma comment(lib, "d3d11.lib")

CaptureManager::CaptureManager()
	: mInitialized(false)
	, mDevice(nullptr)
	, mDxgiAdapter(nullptr)
	, mDxgiOutput(nullptr)
	, mDxgiOutput1(nullptr)
	, mDuplication(nullptr)
{
}

CaptureManager::~CaptureManager()
{
	if (mDuplication != nullptr)
	{
		mDuplication->ReleaseFrame();
		mDuplication->Release();
	}
	if (mDxgiOutput != nullptr)
	{
		mDxgiOutput->Release();
	}
}

HRESULT CaptureManager::Init(ID3D11Device* device, IDXGIAdapter* dxgiAdapter)
{
	HRESULT hr = S_OK;

	mDevice = device;
	mDxgiAdapter = dxgiAdapter;

	assert(mDevice);
	assert(mDxgiAdapter);

	hr = mDxgiAdapter->EnumOutputs(0, &mDxgiOutput);
	if (hr != S_OK)
	{
		return hr;
	}

	hr = mDxgiOutput->QueryInterface<IDXGIOutput1>(&mDxgiOutput1);
	if (hr != S_OK)
	{
		return hr;
	}

	hr = mDxgiOutput1->DuplicateOutput(mDevice, &mDuplication);
	if (hr != S_OK)
	{
		return hr;
	}

	mInitialized = true;
	return hr;
}

HRESULT CaptureManager::AcquireFrameAsTexture(ID3D11Texture2D** tex)
{
	assert(mInitialized);

	IDXGIResource* displayResource;
	DXGI_OUTDUPL_FRAME_INFO frameInfo;

	HRESULT hr = S_OK;

	// first release the frame (if we are currently holding it)
	hr = mDuplication->ReleaseFrame();
	// INVALID_CALL is probably ok, because it means the frame was already released
	if (hr != S_OK && hr != DXGI_ERROR_INVALID_CALL)
	{
		if (hr == DXGI_ERROR_ACCESS_LOST)
		{
			return hr;
		}
	}

	hr = mDuplication->AcquireNextFrame(INFINITE, &frameInfo, &displayResource);
	if (hr != S_OK)
	{
		return hr;
	}

	hr = displayResource->QueryInterface<ID3D11Texture2D>(tex);
	return hr;
}

HRESULT CaptureManager::ReleaseFrame()
{
	HRESULT hr = mDuplication->ReleaseFrame();
	if (hr != S_OK && hr != DXGI_ERROR_INVALID_CALL)
	{
		return hr;
	}
	return S_OK;
}
