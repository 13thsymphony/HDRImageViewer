#include "pch.h"
#include "ImageLoader.h"
#include "Common\DirectXHelper.h"
#include "DirectXTex.h"
#include "DirectXTex\DirectXTexEXR.h"

using namespace HDRImageViewer;

using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Platform;
using namespace std;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;

static const unsigned int sc_MaxBytesPerPixel = 16; // Covers all supported image formats (128bpp).

ImageLoader::ImageLoader(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources),
    m_state(ImageLoaderState::NotInitialized)
{
}

ImageLoader::~ImageLoader()
{
}

/// <summary>
/// Performs CPU-side decoding of an image using WIC and reads key image parameters.
/// </summary>
ImageInfo ImageLoader::LoadImageFromWic(_In_ IStream* imageStream)
{
    EnforceStates(1, ImageLoaderState::NotInitialized);

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

/// <summary>
/// Performs CPU-side decoding of an image using DirectXTex and reads key image parameters.
/// </summary>
/// <remarks>
/// Supports OpenEXR, Radiance RGBE, and certain DDS files - this is designed for a Direct2D-based
/// renderer, so we use WIC as an intermediate step which only supports some DDS DXGI_FORMAT values.
/// </remarks>
/// <param name="filename">The file path must be accessible from the sandbox, e.g. from the app's temp folder.</param>
/// <param name="extension">File extension with leading period. Needed as DirectXTex doesn't auto-detect codec type.</param>
ImageInfo ImageLoader::LoadImageFromDirectXTex(String^ filename, String^ extension)
{
    EnforceStates(1, ImageLoaderState::NotInitialized);

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

/// <summary>
/// After initial decode, obtains image information and do common setup.
/// Populates all members of ImageInfo.
/// </summary>
/// <param name="source">Ideally IWICBitmapFrameDecode, otherwise some ImageInfo will not be populated.</param>
void ImageLoader::LoadImageCommon(_In_ IWICBitmapSource* source)
{
    EnforceStates(1, ImageLoaderState::NotInitialized);

    auto wicFactory = m_deviceResources->GetWicImagingFactory();
    m_imageInfo = {};

    // Attempt to read the embedded color profile from the image; only valid for WIC images.
    ComPtr<IWICBitmapFrameDecode> frame;
    if (SUCCEEDED(source->QueryInterface(IID_PPV_ARGS(&frame))))
    {
        DX::ThrowIfFailed(
            wicFactory->CreateColorContext(&m_wicColorContext)
        );

        DX::ThrowIfFailed(
            frame->GetColorContexts(
                1,
                m_wicColorContext.GetAddressOf(),
                &m_imageInfo.numProfiles
            )
        );
    }

    // Check whether the image data is natively stored in a floating-point format, and
    // decode to the appropriate WIC pixel format.

    WICPixelFormatGUID pixelFormat;
    DX::ThrowIfFailed(
        source->GetPixelFormat(&pixelFormat)
    );

    ComPtr<IWICComponentInfo> componentInfo;
    DX::ThrowIfFailed(
        wicFactory->CreateComponentInfo(
            pixelFormat,
            &componentInfo
        )
    );

    ComPtr<IWICPixelFormatInfo2> pixelFormatInfo;
    DX::ThrowIfFailed(
        componentInfo.As(&pixelFormatInfo)
    );

    WICPixelFormatNumericRepresentation formatNumber;
    DX::ThrowIfFailed(
        pixelFormatInfo->GetNumericRepresentation(&formatNumber)
    );

    DX::ThrowIfFailed(pixelFormatInfo->GetBitsPerPixel(&m_imageInfo.bitsPerPixel));

    // Calculate the bits per channel (bit depth) using GetChannelMask.
    // This accounts for nonstandard color channel packing and padding, e.g. 32bppRGB,
    // but assumes each channel has equal bits (e.g. RGB565 doesn't work).
    unsigned char channelMaskBytes[sc_MaxBytesPerPixel];
    ZeroMemory(channelMaskBytes, ARRAYSIZE(channelMaskBytes));
    unsigned int maskSize;

    DX::ThrowIfFailed(
        pixelFormatInfo->GetChannelMask(
            0,  // Read the first color channel.
            ARRAYSIZE(channelMaskBytes),
            channelMaskBytes,
            &maskSize)
    );

    // Count up the number of bits set in the mask for the first color channel.
    for (unsigned int i = 0; i < maskSize * 8; i++)
    {
        unsigned int byte = i / 8;
        unsigned int bit = i % 8;
        if ((channelMaskBytes[byte] & (1 << bit)) != 0)
        {
            m_imageInfo.bitsPerChannel += 1;
        }
    }

    m_imageInfo.isFloat = (WICPixelFormatNumericRepresentationFloat == formatNumber) ? true : false;

    // When decoding, preserve the numeric representation (float vs. non-float)
    // of the native image data. This avoids WIC performing an implicit gamma conversion
    // which occurs when converting between a fixed-point/integer pixel format (sRGB gamma)
    // and a float-point pixel format (linear gamma). Gamma adjustment, if specified by
    // the ICC profile, will be performed by the Direct2D color management effect.

    WICPixelFormatGUID fmt = {};
    if (m_imageInfo.isFloat)
    {
        fmt = GUID_WICPixelFormat64bppPRGBAHalf; // Equivalent to DXGI_FORMAT_R16G16B16A16_FLOAT.
    }
    else
    {
        fmt = GUID_WICPixelFormat64bppPRGBA; // Equivalent to DXGI_FORMAT_R16G16B16A16_UNORM.
                                             // Many SDR images (e.g. JPEG) use <=32bpp, so it
                                             // is possible to further optimize this for memory usage.
    }

    DX::ThrowIfFailed(
        wicFactory->CreateFormatConverter(&m_formatConvert)
    );

    DX::ThrowIfFailed(
        m_formatConvert->Initialize(
            source,
            fmt,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0f,
            WICBitmapPaletteTypeCustom
        )
    );

    UINT width;
    UINT height;
    DX::ThrowIfFailed(
        m_formatConvert->GetSize(&width, &height)
    );

    m_imageInfo.size = Size(static_cast<float>(width), static_cast<float>(height));

    PopulateImageInfoACKind(&m_imageInfo, source);

    m_imageInfo.isValid = true;
    m_state = ImageLoaderState::NeedDeviceResources;

    CreateDeviceDependentResourcesInternal();
}

/// <summary>
/// (Re)initializes all long-lived device dependent resources.
/// </summary>
void ImageLoader::CreateDeviceDependentResourcesInternal()
{
    EnforceStates(2, ImageLoaderState::NotInitialized, ImageLoaderState::NeedDeviceResources);

    auto d2dFactory = m_deviceResources->GetD2DFactory();
    auto context = m_deviceResources->GetD2DDeviceContext();

    // Load the image from WIC using ID2D1ImageSource.
    DX::ThrowIfFailed(
        m_deviceResources->GetD2DDeviceContext()->CreateImageSourceFromWic(
            m_formatConvert.Get(),
            &m_imageSource));

    // Xbox One HDR screenshots use the HDR10 colorspace, and this must be manually specified.
    if (m_imageInfo.isXboxHdrScreenshot)
    {
        ComPtr<ID2D1ColorContext1> colorContext1;
        DX::ThrowIfFailed(
            context->CreateColorContextFromDxgiColorSpace(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, &colorContext1));

        DX::ThrowIfFailed(colorContext1.As(&m_colorContext));
    }
    // If the image contains an embedded color profile, use it.
    else if (m_imageInfo.numProfiles >= 1)
    {
        DX::ThrowIfFailed(
            context->CreateColorContextFromWicColorContext(
                m_wicColorContext.Get(),
                &m_colorContext));
    }
    // If no other info is available, select a default color profile based on pixel format:
    // floating point == scRGB, others == sRGB.
    else
    {
        DX::ThrowIfFailed(
            context->CreateColorContext(
                m_imageInfo.isFloat ? D2D1_COLOR_SPACE_SCRGB : D2D1_COLOR_SPACE_SRGB,
                nullptr,
                0,
                &m_colorContext));
    }

    m_state = ImageLoaderState::LoadingSucceeded;
}

/// <summary>
/// Gets the Direct2D image representing decoded image data.
/// </summary>
/// <remarks>Call this every time a new zoom factor is desired.</remarks>
ID2D1TransformedImageSource* ImageLoader::GetLoadedImage(float zoom)
{
    EnforceStates(1, ImageLoaderState::LoadingSucceeded);

    // When using ID2D1ImageSource, the recommend method of scaling is to use
    // ID2D1TransformedImageSource. It is inexpensive to recreate this object.
    D2D1_TRANSFORMED_IMAGE_SOURCE_PROPERTIES props =
    {
        D2D1_ORIENTATION_DEFAULT,
        zoom,
        zoom,
        D2D1_INTERPOLATION_MODE_LINEAR, // This is ignored when using DrawImage.
        D2D1_TRANSFORMED_IMAGE_SOURCE_OPTIONS_NONE
    };

    ComPtr<ID2D1TransformedImageSource> source;

    DX::ThrowIfFailed(
        m_deviceResources->GetD2DDeviceContext()->CreateTransformedImageSource(
            m_imageSource.Get(),
            &props,
            &source
        )
    );

    return source.Detach();
}

/// <summary>
/// Gets the color context of the image.
/// </summary>
ID2D1ColorContext* ImageLoader::GetImageColorContext()
{
    EnforceStates(1, ImageLoaderState::LoadingSucceeded);

    // Do NOT call GetImageColorContextInternal - it was already called by LoadImageCommon.
    return m_colorContext.Get();
}

/// <summary>
/// Gets ImageInfo.
/// </summary>
ImageInfo ImageLoader::GetImageInfo()
{
    EnforceStates(2, ImageLoaderState::LoadingSucceeded, ImageLoaderState::NeedDeviceResources);

    return m_imageInfo;
}

/// <summary>
/// Recreates device resources after device lost.
/// </summary>
/// <remarks>
/// ImageLoader doesn't implement IDeviceNotify and relies on the caller to tell it
/// when device resources need to be recreated.
/// Don't call this during normal image load/initialization as this is done automatically.
/// </remarks>
void ImageLoader::CreateDeviceDependentResources()
{
    // Device lost/restored can occur at any time.
    switch (m_state)
    {
    case ImageLoaderState::NotInitialized:
    case ImageLoaderState::LoadingFailed:
        // No-op if there is nothing to be rendered.
        break;

    case ImageLoaderState::NeedDeviceResources:
        CreateDeviceDependentResourcesInternal();
        break;

    case ImageLoaderState::LoadingSucceeded:
    default:
        throw ref new COMException(WINCODEC_ERR_WRONGSTATE);
        break;
    }
}

/// <summary>
/// Releases (invalid) device resources after device lost.
/// </summary>
/// <remarks>
/// ImageLoader doesn't implement IDeviceNotify and relies on the caller to tell it
/// when device resources need to be recreated.
/// </remarks>
void ImageLoader::ReleaseDeviceDependentResources()
{
    // Device lost/restored can occur at any time.
    switch (m_state)
    {
    case ImageLoaderState::NotInitialized:
    case ImageLoaderState::LoadingFailed:
        // No-op if there is nothing to be rendered.
        break;

    case ImageLoaderState::LoadingSucceeded:
        m_state = ImageLoaderState::NeedDeviceResources;

        m_imageSource.Reset();
        m_colorContext.Reset();
        break;

    case ImageLoaderState::NeedDeviceResources:
    default:
        throw ref new COMException(WINCODEC_ERR_WRONGSTATE);
        break;
    }
}

/// <summary>
/// Determines what advanced color kind the image is.
/// </summary>
/// <param name="info">Requires that all fields other than imageKind are already populated.</param>
/// <param name="source">For some detection types, IWICBitmapFrameDecode is needed.</param>
void ImageLoader::PopulateImageInfoACKind(_Inout_ ImageInfo* info, _In_ IWICBitmapSource* source)
{
    if (info->bitsPerPixel == 0 ||
        info->bitsPerChannel == 0 ||
        info->size.Width == 0 ||
        info->size.Height == 0)
    {
        DX::ThrowIfFailed(E_INVALIDARG);
    }

    info->imageKind = AdvancedColorKind::StandardDynamicRange;

    // Bit depth > 8bpc or color gamut > sRGB signifies a WCG image.
    // The presence of a color profile is used as an approximation for wide gamut.
    if (info->bitsPerChannel > 8 || info->numProfiles >= 1)
    {
        info->imageKind = AdvancedColorKind::WideColorGamut;
    }

    // Currently, all supported floating point images are considered HDR.
    if (info->isFloat == true)
    {
        info->imageKind = AdvancedColorKind::HighDynamicRange;
    }

    // Xbox One HDR screenshots have to be specially detected.
    if (IsImageXboxHdrScreenshot(source))
    {
        m_imageInfo.imageKind = AdvancedColorKind::HighDynamicRange;
        m_imageInfo.isXboxHdrScreenshot = true;
    }
}

/// <summary>
/// Detects if the image is an Xbox One HDR screenshot.
/// </summary>
/// <remarks>
/// Xbox One HDR screenshots use JPEG XR with 10-bit precision and the HDR10 colorspace, however they are
/// indistinguishable from SDR/sRGB 10-bit JPEG XRs except for custom XMP metadata embedded in them.
/// </remarks>
/// <param name="source">Must be an IWICBitmapFrameDecode</param>
bool ImageLoader::IsImageXboxHdrScreenshot(IWICBitmapSource* source)
{
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(source->QueryInterface(IID_PPV_ARGS(&frame))))
    {
        return false;
    }

    // Eventually should detect whether the codec actually is JPEG XR. This requires the IWICBitmapDecoder.

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


/// <summary>
/// Translates DXGI_FORMAT to the best equivalent WIC pixel format.
/// </summary>
/// <remarks>
/// Returns GUID_WICPixelFormatUndefined if we don't know the right WIC pixel format.
/// This list is highly incomplete and only covers the most important DXGI_FORMATs for HDR.
/// </remarks>
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
