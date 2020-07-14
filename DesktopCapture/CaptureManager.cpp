#include "pch.h"
#include "CaptureManager.h"

#include <d3d11.h>
//#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_5.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
//#pragma comment(lib, "d3d12.lib")
//#pragma comment(lib, "dxgi.lib")

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

HRESULT CaptureManager::Init()
{
	HRESULT hr = S_OK;

	D3D_FEATURE_LEVEL FeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_12_0
	};

	D3D_FEATURE_LEVEL availableFeatureLevel;
	hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		FeatureLevels,
		1,
		D3D11_SDK_VERSION,
		&mDevice,
		&availableFeatureLevel,
		&mContext);
	assert(SUCCEEDED(hr));
	
	IDXGIDevice3* dxgiDevice;
	hr = mDevice->QueryInterface<IDXGIDevice3>(&dxgiDevice);
	assert(SUCCEEDED(hr));

	IDXGIAdapter* adapter;
	hr = dxgiDevice->GetAdapter(&adapter);
	assert(SUCCEEDED(hr));

	hr = adapter->QueryInterface<IDXGIAdapter1>(&mDxgiAdapter);
	assert(SUCCEEDED(hr));

	IDXGIFactory2* factory;
	hr = mDxgiAdapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&factory));
	assert(SUCCEEDED(hr));

	hr = mDxgiAdapter->EnumOutputs(0, &mDxgiOutput);
	assert(SUCCEEDED(hr));
	if (hr != S_OK)
	{
		return hr;
	}

	hr = mDxgiOutput->QueryInterface<IDXGIOutput5>(&mDxgiOutput1);
	assert(SUCCEEDED(hr));
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

HRESULT CaptureManager::AcquireFrameAsDXGIResource(IDXGIResource** frameResource)
{
	assert(mInitialized);
	DXGI_OUTDUPL_FRAME_INFO frameInfo;

	// first release the frame (if we are currently holding it)
	HRESULT hr = mDuplication->ReleaseFrame();
	// INVALID_CALL is probably ok, because it means the frame was already released
	if (hr != S_OK && hr != DXGI_ERROR_INVALID_CALL)
	{
		frameResource = nullptr;
		return hr;
	}

	hr = mDuplication->AcquireNextFrame(INFINITE, &frameInfo, frameResource);
	return hr;
}

HRESULT CaptureManager::AcquireFrameAsTexture(ID3D11Texture2D** tex)
{
	IDXGIResource* displayResource;
	HRESULT hr = AcquireFrameAsDXGIResource(&displayResource);
	if (SUCCEEDED(hr))
	{
		hr = displayResource->QueryInterface<ID3D11Texture2D>(tex);
	}
	return hr;
}

HRESULT CaptureManager::AcquireFrameAsSurface(IDXGISurface** surface)
{
	IDXGIResource* displayResource;
	HRESULT hr = AcquireFrameAsDXGIResource(&displayResource);
	if (SUCCEEDED(hr))
	{
		hr = displayResource->QueryInterface<IDXGISurface>(surface);
	}
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
