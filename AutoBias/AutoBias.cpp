// AutoBias.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <d3d11.h>
#include <dxgi1_2.h>

#pragma comment(lib, "d3d11.lib")

enum class ErrorCodes
{
    D3DDeviceCreate = 1,
    DXGIDeviceCreate,
    DXGIAdapterFetch,
    DXGIOutputFetch,
    DXGIOutput1Fetch,
    DXGISurfaceFetch,
    DXGIDuplication,
    FrameAcquisition,
    ResourceToTexture
};

template <typename EnumType>
int EnumToValue(EnumType e)
{
    return static_cast<int>(e);
}

int main()
{
    std::cout << "Hello World!\n";

    HRESULT hr = S_OK;

    ID3D11Device* device;
    ID3D11DeviceContext* context;
    IDXGIDevice* dxgiDevice;
    IDXGIAdapter* dxgiAdapter;
    IDXGIOutput* dxgiOutput;
    IDXGIOutput1* dxgiOutput1;
    IDXGIOutputDuplication* duplication;
    ID3D11Texture2D* desktopSurfaceTexture;
    //IDXGIResource dxgiSurfaceResource;

    D3D_FEATURE_LEVEL FeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0
    };

    D3D_FEATURE_LEVEL availableFeatureLevel;

    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, FeatureLevels, 1, D3D11_SDK_VERSION, &device, &availableFeatureLevel, &context);

    if (!SUCCEEDED(hr))
    {
        std::cerr << "Failed to create D3D11 Device and Context" << std::endl;
        return EnumToValue(ErrorCodes::D3DDeviceCreate);
    }

    hr = device->QueryInterface<IDXGIDevice>(&dxgiDevice);

    if (!SUCCEEDED(hr))
    {
        std::cerr << "Failed to create DXGI Device" << std::endl;
        return EnumToValue(ErrorCodes::DXGIDeviceCreate);
    }

    //hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&DxgiAdapter));
    hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter));

    if (!SUCCEEDED(hr))
    {
        std::cerr << "Failed to obtain DXGI Adapter" << std::endl;
        return EnumToValue(ErrorCodes::DXGIAdapterFetch);
    }

    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);

    if (!SUCCEEDED(hr))
    {
        std::cerr << "Failed to obtain DXGI Output" << std::endl;
        return EnumToValue(ErrorCodes::DXGIOutputFetch);
    }

    hr = dxgiOutput->QueryInterface<IDXGIOutput1>(&dxgiOutput1);

    if (!SUCCEEDED(hr))
    {
        std::cerr << "Failed to obtain DXGI Output 1" << std::endl;
        return EnumToValue(ErrorCodes::DXGIOutput1Fetch);
    }

    hr = dxgiOutput1->DuplicateOutput(device, &duplication);

    if (!SUCCEEDED(hr))
    {
        std::cerr << "Failed to duplicate output" << std::endl;
        return EnumToValue(ErrorCodes::DXGIDuplication);
    }


    IDXGIResource* displayResource;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;

    hr = duplication->AcquireNextFrame(INFINITE, &frameInfo, &displayResource);
    if (!SUCCEEDED(hr))
    {
        return hr;
    }

    hr = displayResource->QueryInterface<ID3D11Texture2D>(&desktopSurfaceTexture);
    if (!SUCCEEDED(hr))
    {
        return hr;
    }

    //hr = dxgiOutput1->GetDisplaySurfaceData1(&dxgiSurfaceResource);

    //if (!SUCCEEDED(hr))
    //{
    //    std::cerr << "Failed to fetch display surface" << std::endl;
    //    return EnumToValue(ErrorCodes::DXGISurfaceFetch);
    //}
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
