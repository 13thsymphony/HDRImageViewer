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

#include "pch.h"
#include "ImageLoader.h"
#include "DirectXHelper.h"
#include "DirectXTex.h"
#include "DirectXTex\DirectXTexEXR.h"

using namespace D2DAdvancedColorImages;

using namespace concurrency;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Platform;
using namespace std;

ImageLoader::ImageLoader(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources),
    m_state(ImageLoaderState::NotInitialized)
{
}

ImageLoader::~ImageLoader()
{
}

// Reads the provided data stream and decodes an image from it using WIC. These resources are device-independent.
ImageInfo ImageLoader::LoadImageFromWic(_In_ IStream* imageStream)
{
    if (m_state != ImageLoaderState::NotInitialized)
    {
        throw ref new COMException(WINCODEC_ERR_WRONGSTATE);
    }

    auto wicFactory = m_deviceResources->GetWicImagingFactory();

    // Decode the image using WIC.
    ComPtr<IWICBitmapDecoder> decoder;
    DX::ThrowIfFailed(
        wicFactory->CreateDecoderFromStream(
            imageStream,
            nullptr,
            WICDecodeMetadataCacheOnDemand,
            &decoder
        ));

    ComPtr<IWICBitmapFrameDecode> frame;
    DX::ThrowIfFailed(
        decoder->GetFrame(0, &frame)
    );

    LoadImageCommon(frame.Get());

    return m_imageInfo;
}

// Relies on the file path being accessible from the sandbox, e.g. from the app's temp folder.
// Supports OpenEXR, Radiance RGBE, and certain DDS files - because the renderer uses Direct2D
// we use WIC as an intermediary for image loading, which restricts the set of supported DDS
// DXGI_FORMAT values.
// Also relies on the correct file extension, as DirectXTex doesn't auto-detect codec type.
ImageInfo ImageLoader::LoadImageFromDirectXTex(String^ filename, String^ extension)
{
    ComPtr<IWICBitmapSource> decodedSource;

    auto dxtScratch = new ScratchImage();
    auto filestr = filename->Data();

    if (extension == L".EXR" || extension == L".exr")
    {
        DX::ThrowIfFailed(LoadFromEXRFile(filestr, nullptr, *dxtScratch));
    }
    else if (extension == L".HDR" || extension == L".hdr")
    {
        DX::ThrowIfFailed(LoadFromHDRFile(filestr, nullptr, *dxtScratch));
    }
    else
    {
        DX::ThrowIfFailed(LoadFromDDSFile(filestr, 0, nullptr, *dxtScratch));
    }

    auto image = dxtScratch->GetImage(0, 0, 0); // Always get the first image.

    // Decompress if the image uses block compression. This does not use WIC and Direct2D's
    // native support for BC1, BC2, and BC3 formats.
    auto decompScratch = new ScratchImage();
    if (DirectX::IsCompressed(image->format))
    {
        DX::ThrowIfFailed(
            DirectX::Decompress(*image, DXGI_FORMAT_UNKNOWN, *decompScratch)
        );

        // Memory for each Image is managed by ScratchImage.
        image = decompScratch->GetImage(0, 0, 0);
    }

    GUID wicFmt = TranslateDxgiFormatToWic(image->format);
    if (wicFmt == GUID_WICPixelFormatUndefined)
    {
        // We don't know how to load in WIC, so just fail.
        return m_imageInfo;
    }

    ComPtr<IWICBitmap> dxtWicBitmap;
    auto fact = m_deviceResources->GetWicImagingFactory();
    DX::ThrowIfFailed(
        fact->CreateBitmapFromMemory(
            static_cast<UINT>(image->width),
            static_cast<UINT>(image->height),
            wicFmt,
            static_cast<UINT>(image->rowPitch),
            static_cast<UINT>(image->slicePitch),
            image->pixels,
            &dxtWicBitmap
        )
    );

    LoadImageCommon(dxtWicBitmap.Get());

    // TODO: Common code to check file type?
    if (extension == L".HDR" || extension == L".hdr")
    {
        // Manually fix up Radiance RGBE image file bit depth as DirectXTex expands it to 128bpp.
        // 16 bpc is not strictly accurate but best preserves the intent of RGBE.
        m_imageInfo.bitsPerPixel = 32;
        m_imageInfo.bitsPerChannel = 16;
    }

    return m_imageInfo;
}

// Only valid if state is ImageLoaderState::LoadingSucceeded.
ID2D1TransformedImageSource * D2DAdvancedColorImages::ImageLoader::GetLoadedImage(float zoom)
{
    return nullptr;
}

// XBox One HDR screenshots are captured using JPEG XR with a 10-bit pixel format and custom XMP metadata.
// They use the HDR10 colorspace but this is not explicitly stored in the file, so we must manually
// detect them. Requires source to be IWICBitmapFrameDecode.
bool ImageLoader::IsImageXboxHdrScreenshot(IWICBitmapSource* source)
{
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(source->QueryInterface(IID_PPV_ARGS(&frame))))
    {
        return false;
    }

    // Eventually should detect whether the codec actually is JPEG XR.

    WICPixelFormatGUID fmt = {};
    DX::ThrowIfFailed(frame->GetPixelFormat(&fmt));
    if (fmt != GUID_WICPixelFormat32bppBGR101010)
    {
        return false;
    }

    ComPtr<IWICMetadataQueryReader> metadata;
    if (FAILED(frame->GetMetadataQueryReader(&metadata)))
    {
        // If metadata is not supported, this returns WINCODEC_ERR_UNSUPPORTEDOPERATION.
        return false;
    }

    PROPVARIANT prop;
    PropVariantInit(&prop);
    if (FAILED(metadata->GetMetadataByName(L"/ifd/xmp/{wstr=http://ns.microsoft.com/gamedvr/1.0/}:Extended", &prop)))
    {
        // If the Xbox-specific metadata is not found, this returns WINCODEC_ERR_PROPERTYNOTFOUND.
        PropVariantClear(&prop);
        return false;
    }

    return true;
}

// Returns GUID_WICPixelFormatUndefined if we don't know the right WIC pixel format.
// This list is highly incomplete and only covers the most important DXGI_FORMATs for HDR.
GUID ImageLoader::TranslateDxgiFormatToWic(DXGI_FORMAT fmt)
{
    switch (fmt)
    {
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
        return GUID_WICPixelFormat32bppRGBA;
        break;

    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        // Used by OpenEXR.
        return GUID_WICPixelFormat64bppRGBAHalf;
        break;

    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        // Used by Radiance RGBE; specifically DirectXTex expands out to FP32
        // even though WIC offers a native GUID_WICPixelFormat32bppRGBE.
        return GUID_WICPixelFormat128bppRGBAFloat;
        break;

    default:
        return GUID_WICPixelFormatUndefined;
        break;
    }
}
