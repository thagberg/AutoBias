#pragma once

#include <d3d11.h>

struct IDXGIAdapter1;
struct IDXGIOutput5;

class CaptureManager
{
public:
	__declspec(dllexport) CaptureManager();
    __declspec(dllexport) ~CaptureManager();

    __declspec(dllexport) HRESULT Init();

    __declspec(dllexport) HRESULT __stdcall AcquireFrameAsTexture(ID3D11Texture2D** tex);

    __declspec(dllexport) HRESULT __stdcall ReleaseFrame();

private:
    bool mInitialized;
	ID3D11Device* mDevice;
    ID3D11DeviceContext* mContext;
    IDXGIAdapter1* mDxgiAdapter;
    IDXGIOutput* mDxgiOutput;
    IDXGIOutput5* mDxgiOutput1;
    IDXGIOutputDuplication* mDuplication;
};
