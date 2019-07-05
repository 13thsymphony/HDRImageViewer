//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include <ppltasks.h>

namespace DX
{
    // TODO: Migrate to just use this shorter identifier.
#define CHK ThrowIfFailed

    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            // Set a breakpoint on this line to catch Win32 API errors.
            throw Platform::Exception::CreateException(hr);
        }
    }

    enum OSVer
    {
        Win1803,
        Win1809
    };

    // Check for OS version. Common helper function makes it easier to do testing/mockups.
    // DirectX doesn't allow us to check for a specific feature's presence, so instead
    // we check the OS version (API contract).
    inline bool CheckPlatformSupport(OSVer version)
    {
        int apiLevel = 0;
        switch (version)
        {
        case Win1803:
            apiLevel = 6;
            break;

        case Win1809:
            apiLevel = 7;
            break;

        default:
            throw ref new Platform::InvalidArgumentException();
            break;
        }

        return Windows::Foundation::Metadata::ApiInformation::IsApiContractPresent(
            "Windows.Foundation.UniversalApiContract", apiLevel);
    }

    // Function that reads from a binary file asynchronously.
    inline Concurrency::task<std::vector<byte>> ReadDataAsync(const std::wstring& filename)
    {
        using namespace Windows::Storage;
        using namespace Concurrency;

        auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;

        return create_task(folder->GetFileAsync(Platform::StringReference(filename.c_str()))).then([] (StorageFile^ file) 
        {
            return FileIO::ReadBufferAsync(file);
        }).then([] (Streams::IBuffer^ fileBuffer) -> std::vector<byte> 
        {
            std::vector<byte> returnBuffer;
            returnBuffer.resize(fileBuffer->Length);
            Streams::DataReader::FromBuffer(fileBuffer)->ReadBytes(Platform::ArrayReference<byte>(returnBuffer.data(), fileBuffer->Length));
            return returnBuffer;
        });
    }

    // Converts a length in device-independent pixels (DIPs) to a length in physical pixels.
    inline float ConvertDipsToPixels(float dips, float dpi)
    {
        static const float dipsPerInch = 96.0f;
        return floorf(dips * dpi / dipsPerInch + 0.5f); // Round to nearest integer.
    }

    // Converts a length in physical pixels to a length in device-independent pixels (DIPs).
    inline float ConvertPixelsToDips(float pixels)
    {
        static const float dipsPerInch = 96.0f;
        return pixels * dipsPerInch / Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi; // Do not round.
    }

    // Converts a length in physical pixels to a length in device-independent pixels (DIPs) using the provided DPI. This removes the need
    // to call this function from a thread associated with a CoreWindow, which is required by DisplayInformation::GetForCurrentView().
    inline float ConvertPixelsToDips(float pixels, float dpi)
    {
        static const float dipsPerInch = 96.0f;
        return pixels * dipsPerInch / dpi; // Do not round.
    }

#if defined(_DEBUG)
    // Check for SDK Layer support.
    inline bool SdkLayersAvailable()
    {
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
            0,
            D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
            nullptr,                    // Any feature level will do.
            0,
            D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Runtime apps.
            nullptr,                    // No need to keep the D3D device reference.
            nullptr,                    // No need to know the feature level.
            nullptr                     // No need to keep the D3D device context reference.
            );

        return SUCCEEDED(hr);
    }
#endif
}
