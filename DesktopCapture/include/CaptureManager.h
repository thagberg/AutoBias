#pragma once

#include <d3d11.h>

struct IDXGIAdapter1;
struct IDXGIOutput5;
struct IDXGIOutputDuplication;

class CaptureManager
{
public:
	__declspec(dllexport) CaptureManager();
    __declspec(dllexport) ~CaptureManager();

    __declspec(dllexport) HRESULT Init();

    __declspec(dllexport) HRESULT __stdcall AcquireFrameAsDXGIResource(IDXGIResource** frameResource) const;

    __declspec(dllexport) HRESULT __stdcall AcquireFrameAsTexture(ID3D11Texture2D** tex) const;

    __declspec(dllexport) HRESULT __stdcall AcquireFrameAsSurface(IDXGISurface** surface) const;

    __declspec(dllexport) HRESULT __stdcall ReleaseFrame() const;

private:
    bool mInitialized;
	ID3D11Device* mDevice;
    ID3D11DeviceContext* mContext;
    IDXGIAdapter1* mDxgiAdapter;
    IDXGIOutput* mDxgiOutput;
    IDXGIOutput5* mDxgiOutput1;
    IDXGIOutputDuplication* mDuplication;
};
