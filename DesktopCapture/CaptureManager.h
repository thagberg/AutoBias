#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;

class CaptureManager
{
public:
	__declspec(dllexport) CaptureManager();
    __declspec(dllexport) ~CaptureManager();

    __declspec(dllexport) HRESULT Init(ID3D11Device* device, IDXGIAdapter* dxgiAdapter);

    __declspec(dllexport) HRESULT __stdcall AcquireFrameAsTexture(ID3D11Texture2D** tex);

    __declspec(dllexport) HRESULT __stdcall ReleaseFrame();

private:
    bool mInitialized;
	ID3D11Device* mDevice;
    IDXGIAdapter* mDxgiAdapter;
    IDXGIOutput* mDxgiOutput;
    IDXGIOutput1* mDxgiOutput1;
    IDXGIOutputDuplication* mDuplication;
};
