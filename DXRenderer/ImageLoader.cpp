#include "pch.h"
#include "ImageLoader.h"
#include "Common\DirectXHelper.h"
#include "DirectXTex.h"
#include "DirectXTex\DirectXTexEXR.h"

using namespace DXRenderer;

using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Platform;
using namespace std;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;

static const unsigned int sc_MaxBytesPerPixel = 16; // Covers all supported image formats (128bpp).

ImageLoader::ImageLoader(const std::shared_ptr<DeviceResources>& deviceResources, ImageLoaderOptions options) :
    m_deviceResources(deviceResources),
    m_state(ImageLoaderState::NotInitialized),
    m_imageInfo{},
    m_options(options),
    // Data extracted from Xbox console HDR screen capture image
    m_xboxHdrIccSize(2676),
    m_xboxHdrIccHeaderBytes {
        0x00, 0x00, 0x0A, 0x74, 0x00, 0x00, 0x00, 0x00, 0x02, 0x40, 0x00, 0x00,
        0x6D, 0x6E, 0x74, 0x72, 0x52, 0x47, 0x42, 0x20, 0x58, 0x59, 0x5A, 0x20,
        0x07, 0xE1, 0x00, 0x08, 0x00, 0x1E, 0x00, 0x0C, 0x00, 0x06, 0x00, 0x34,
        0x61, 0x63, 0x73, 0x70, 0x4D, 0x53, 0x46, 0x54, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF6, 0xD6,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xD3, 0x2D, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }
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
    LoadImageFromWicInt(imageStream);

    return m_imageInfo;
}

/// <summary>
/// Internal method is needed because IFRIMG macro methods must return void.
/// If any failure occurs during image loading, immediately exits with
/// m_state and imageinfo set to failed.
/// </summary>
void ImageLoader::LoadImageFromWicInt(_In_ IStream* imageStream)
{
    EnforceStates(1, ImageLoaderState::NotInitialized);

    auto wicFactory = m_deviceResources->GetWicImagingFactory();

    // Decode the image using WIC.
    ComPtr<IWICBitmapDecoder> decoder;
    IFRIMG(wicFactory->CreateDecoderFromStream(
        imageStream,
        nullptr,
        WICDecodeMetadataCacheOnDemand,
        &decoder));

    ComPtr<IWICBitmapFrameDecode> frame;
    IFRIMG(decoder->GetFrame(0, &frame));

    GUID fmt;
    IFRIMG(decoder->GetContainerFormat(&fmt));

    // Perform initial detection and handling of special case WIC decoders.
    if (fmt == GUID_ContainerFormatHeif)
    {
        m_imageInfo.isHeif = true;

        // HEVC codec is not always installed on the system.
        IFRIMG(CheckCanDecode(frame.Get()) == true ? S_OK : E_FAIL);

        // HEIF/HEVC supports GUID_WICPixelFormat32bppR10G10B10A2HDR10.
        // We must specifically detect and request HDR10 via IWICBitmapSourceTransform.
        ComPtr<IWICBitmapSourceTransform> sourceTransform;
        IFRIMG(frame->QueryInterface(IID_PPV_ARGS(&sourceTransform)));

        GUID checkHDR10Fmt = GUID_WICPixelFormat32bppR10G10B10A2HDR10;
        IFRIMG(sourceTransform->GetClosestPixelFormat(&checkHDR10Fmt));

        if (checkHDR10Fmt == GUID_WICPixelFormat32bppR10G10B10A2HDR10 &&
            m_imageInfo.isHeif == true)
        {
            m_imageInfo.forceBT2100ColorSpace = true;
        }

        HasAppleHdrGainMap(frame.Get(), imageStream);
    }
    else if (fmt == GUID_ContainerFormatWmp)
    {
        // Xbox One HDR screenshots have to be specially detected and are always HDR10/BT.2100.
        if (IsImageXboxHdrScreenshot(frame.Get()))
        {
            m_imageInfo.forceBT2100ColorSpace = true;
        }
    }

    LoadImageCommon(frame.Get());
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
    LoadImageFromDirectXTexInt(filename, extension);

    return m_imageInfo;
}

/// <summary>
/// Internal method is needed because IFRIMG macro methods must return void.
/// If any failure occurs during image loading, immediately exits with
/// m_state and imageinfo set to failed.
/// </summary>
void ImageLoader::LoadImageFromDirectXTexInt(String^ filename, String^ extension)
{
    EnforceStates(1, ImageLoaderState::NotInitialized);

    ComPtr<IWICBitmapSource> decodedSource;

    auto dxtScratch = new ScratchImage();
    auto filestr = filename->Data();

    if (extension == L".EXR" || extension == L".exr")
    {
        IFRIMG(LoadFromEXRFile(filestr, nullptr, *dxtScratch));
    }
    else if (extension == L".HDR" || extension == L".hdr")
    {
        IFRIMG(LoadFromHDRFile(filestr, nullptr, *dxtScratch));
    }
    else
    {
        IFRIMG(LoadFromDDSFile(filestr, DDS_FLAGS_NONE, nullptr, *dxtScratch));
    }

    auto image = dxtScratch->GetImage(0, 0, 0); // Always get the first image.

    // Decompress if the image uses block compression. This does not use WIC and Direct2D's
    // native support for BC1, BC2, and BC3 formats.
    auto decompScratch = new ScratchImage();
    if (DirectX::IsCompressed(image->format))
    {
        IFRIMG(DirectX::Decompress(*image, DXGI_FORMAT_UNKNOWN, *decompScratch));

        // Memory for each Image is managed by ScratchImage.
        image = decompScratch->GetImage(0, 0, 0);
    }

    GUID wicFmt = TranslateDxgiFormatToWic(image->format);

    // Fail if we don't know how to load in WIC.
    IFRIMG(wicFmt == GUID_WICPixelFormatUndefined ? WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT : S_OK);

    ComPtr<IWICBitmap> dxtWicBitmap;
    auto fact = m_deviceResources->GetWicImagingFactory();
    IFRIMG(fact->CreateBitmapFromMemory(
        static_cast<UINT>(image->width),
        static_cast<UINT>(image->height),
        wicFmt,
        static_cast<UINT>(image->rowPitch),
        static_cast<UINT>(image->slicePitch),
        image->pixels,
        &dxtWicBitmap));

    LoadImageCommon(dxtWicBitmap.Get());

    // TODO: Common code to check file type?
    if (extension == L".HDR" || extension == L".hdr")
    {
        // Manually fix up Radiance RGBE image file bit depth as DirectXTex expands it to 128bpp.
        // 16 bpc is not strictly accurate but best preserves the intent of RGBE.
        m_imageInfo.bitsPerPixel = 32;
        m_imageInfo.bitsPerChannel = 16;
    }
}

/// <summary>
/// After initial decode, obtains image information and do common setup.
/// Populates all members of ImageInfo.
/// </summary>
void ImageLoader::LoadImageCommon(_In_ IWICBitmapSource* source)
{
    EnforceStates(1, ImageLoaderState::NotInitialized);

    // Overrides apply to all images
    if (m_options.ForceBT2100 == true)
    {
        m_imageInfo.forceBT2100ColorSpace = true;
    }

    auto wicFactory = m_deviceResources->GetWicImagingFactory();

    WICPixelFormatGUID imageFmt;
    IFRIMG(source->GetPixelFormat(&imageFmt));

    if (m_imageInfo.forceBT2100ColorSpace == true &&
        m_imageInfo.isHeif == true)
    {
        // For compat, IWICBitmapSource::GetPixelFormat() always returns 8bpc,
        // the caller must specifically ask for 10bpc data; see CreateHeifHdr10CpuResources.
        imageFmt = GUID_WICPixelFormat32bppR10G10B10A2HDR10;
    }

    PopulatePixelFormatInfo(m_imageInfo, imageFmt);
    PopulateImageInfoACKind(m_imageInfo, source);

    UINT width;
    UINT height;
    IFRIMG(source->GetSize(&width, &height));
    m_imageInfo.pixelSize = Size(static_cast<float>(width), static_cast<float>(height));

    if (m_imageInfo.isHeif == true &&
        m_imageInfo.forceBT2100ColorSpace == true)
    {
        CreateHeifHdr10CpuResources(source);
    }
    else
    {
        // Attempt to read the embedded color profile from the image; only valid for WIC images.
        ComPtr<IWICBitmapFrameDecode> frame;
        if (SUCCEEDED(source->QueryInterface(IID_PPV_ARGS(&frame))))
        {
            IFRIMG(wicFactory->CreateColorContext(&m_wicColorContext));

            IFRIMG(frame->GetColorContexts(
                1,
                m_wicColorContext.GetAddressOf(),
                &m_imageInfo.numProfiles));
        }

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

        ComPtr<IWICFormatConverter> format;
        IFRIMG(wicFactory->CreateFormatConverter(&format));

        IFRIMG(format->Initialize(
            source,
            fmt,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0f,
            WICBitmapPaletteTypeCustom));

        IFRIMG(format.As(&m_wicCachedSource));
    }

    m_state = ImageLoaderState::NeedDeviceResources;

    CreateDeviceDependentResourcesInternal();

    m_imageInfo.isValid = true;
}

/// <summary>
/// Special codepath to generate a WIC software cache of an HDR10 HEIF image.
/// </summary>
/// <param name="source">Must be a valid HEIF HDR10 IWICBitmapFrameDecode.</param>
/// <remarks>
/// GUID_WICPixelFormat32bppR10G10B10A2HDR10 has very limited support in WIC and D2D, so instead
/// we must create a full resolution WIC bitmap cache and drop into D3D to upload to GPU.
///
/// Needs to be paired with CreateHeifHdr10GpuResources.
/// </remarks>
void ImageLoader::CreateHeifHdr10CpuResources(IWICBitmapSource* source)
{
    // Sanity checks
    IFRIMG(m_imageInfo.isHeif == true ? S_OK : WINCODEC_ERR_INVALIDPARAMETER);
    IFRIMG(m_imageInfo.forceBT2100ColorSpace == true ? S_OK : WINCODEC_ERR_INVALIDPARAMETER);

    auto fact = m_deviceResources->GetWicImagingFactory();

    UINT width, height = 0;
    IFRIMG(source->GetSize(&width, &height));

    ComPtr<IWICBitmapFrameDecode> frame;
    IFRIMG(source->QueryInterface(IID_PPV_ARGS(&frame)));

    ComPtr<IWICBitmapSourceTransform> sourceTransform;
    IFRIMG(frame.As(&sourceTransform));

    GUID hdr10Fmt = GUID_WICPixelFormat32bppR10G10B10A2HDR10;

    ComPtr<IWICBitmap> hdr10Bitmap;
    IFRIMG(fact->CreateBitmap(
        width,
        height,
        hdr10Fmt,
        WICBitmapCacheOnLoad,
        &hdr10Bitmap));

    ComPtr<IWICBitmapLock> lock;
    IFRIMG(hdr10Bitmap->Lock({}, WICBitmapLockWrite, &lock));

    UINT lockStride, lockSize = 0;
    WICInProcPointer lockData = nullptr;
    IFRIMG(lock->GetStride(&lockStride));
    IFRIMG(lock->GetDataPointer(&lockSize, &lockData));

    IFRIMG(sourceTransform->CopyPixels(
        {},
        width,
        height,
        &hdr10Fmt, // Assumes we have already checked GetClosestPixelFormat
        WICBitmapTransformRotate0,
        lockStride,
        lockSize,
        lockData));

    IFRIMG(hdr10Bitmap.As(&m_wicCachedSource));
}

/// <summary>
/// Special codepath to generate a D2D image source backed by an HDR10 HEIF image.
/// </summary>
/// <remarks>
/// GUID_WICPixelFormat32bppR10G10B10A2HDR10 has very limited support in WIC and D2D, so instead
/// we must create a full resolution WIC bitmap cache and drop into D3D to upload to GPU.
///
/// Needs to be paired with CreateHeifHdr10CpuResources.
/// </remarks>
void ImageLoader::CreateHeifHdr10GpuResources()
{
    ComPtr<IWICBitmap> wicBitmap;
    ComPtr<IWICBitmapLock> wicLock;
    IFRIMG(m_wicCachedSource.As(&wicBitmap));
    IFRIMG(wicBitmap->Lock({}, WICBitmapLockRead, &wicLock));

    UINT lockStride, lockSize = 0;
    WICInProcPointer lockData = nullptr;
    IFRIMG(wicLock->GetStride(&lockStride));
    IFRIMG(wicLock->GetDataPointer(&lockSize, &lockData));

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = lockData;
    initData.SysMemPitch = lockStride;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<unsigned int>(m_imageInfo.pixelSize.Width);
    desc.Height = static_cast<unsigned int>(m_imageInfo.pixelSize.Height);
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    auto d3dDevice = m_deviceResources->GetD3DDevice();
    ComPtr<ID3D11Texture2D> tex;
    IFRIMG(d3dDevice->CreateTexture2D(&desc, &initData, tex.GetAddressOf()));

    ComPtr<IDXGISurface> dxgiSurface;
    IFRIMG(tex.As(&dxgiSurface));
    IDXGISurface* arrSurfaces[] = { dxgiSurface.Get() };

    auto context = m_deviceResources->GetD2DDeviceContext();
    
    IFRIMG(context->CreateImageSourceFromDxgi(
        arrSurfaces,
        ARRAYSIZE(arrSurfaces),
        // Image source doesn't support assigning to the BT.2100 color space.
        // Instead, we must do this ourselves in GetImageColorContext().
        DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,
        D2D1_IMAGE_SOURCE_FROM_DXGI_OPTIONS_NONE,
        &m_imageSource));
}

/// <summary>
/// Treats any failures as soft - just returns false and doesn't block further image loading.
/// </summary>
/// <param name="frame"></param>
/// <param name="imageStream"></param>
/// <returns></returns>
bool ImageLoader::HasAppleHdrGainMap(IWICBitmapFrameDecode* frame, IStream* imageStream)
{
    HRESULT hr = S_OK;
    bool result = false;

    ComPtr<IWICMetadataQueryReader> query;
    IFRF(frame->GetMetadataQueryReader(&query));
    CPropVariant prop;
    hr = query->GetMetadataByName(L"System.Photo.CameraManufacturer", &prop);

    // TODO check

    STATSTG stats = {};
    hr = imageStream->Stat(&stats, STATFLAG_NONAME);

    unsigned int sizeBytes = static_cast<unsigned int>(stats.cbSize.QuadPart);

    // Image is too large, give up.
    IFRF(sizeBytes != stats.cbSize.QuadPart ? E_FAIL : S_OK);

    std::vector<byte> fileBuf(sizeBytes);

    ULARGE_INTEGER seeked = {};
    imageStream->Seek({}, STREAM_SEEK_SET, &seeked);

    ULONG cbRead = 0;
    hr = imageStream->Read(fileBuf.data(), static_cast<ULONG>(fileBuf.size()), &cbRead);

    CHeifContext ctx;
    IFRF(HEIFHR(heif_context_read_from_memory_without_copy(ctx.ptr, fileBuf.data(), fileBuf.size(), nullptr)));

    CHeifHandle mainHandle;
    IFRF(HEIFHR(heif_context_get_primary_image_handle(ctx.ptr, &mainHandle.ptr)));

    int countAux = heif_image_handle_get_number_of_auxiliary_images(mainHandle.ptr, 0);
    std::vector<heif_item_id> auxIds(countAux);
    heif_image_handle_get_list_of_auxiliary_image_IDs(mainHandle.ptr, 0, auxIds.data(), auxIds.size());

    for (auto i : auxIds)
    {
        CHeifHandle auxHandle;
        IFRF(HEIFHR(heif_image_handle_get_auxiliary_image_handle(mainHandle.ptr, i, &auxHandle.ptr)));

        CHeifAuxType type;
        IFRF(HEIFHR(heif_image_handle_get_auxiliary_type(auxHandle.ptr, &type.ptr)));

        if (type.IsAppleHdrGainMap())
        {
            IFRF(HEIFHR(heif_decode_image(auxHandle.ptr, &m_appleHdrGainMap.ptr, heif_colorspace_monochrome, heif_chroma_monochrome, 0)));

            int width = heif_image_get_primary_width(m_appleHdrGainMap.ptr);
            int height = heif_image_get_primary_height(m_appleHdrGainMap.ptr);
            int bitdepth = heif_image_get_bits_per_pixel_range(m_appleHdrGainMap.ptr, heif_channel_Y);

            int stride = 0;
            const uint8_t* data = heif_image_get_plane_readonly(m_appleHdrGainMap.ptr, heif_channel_Y, &stride);

            // Immediately return once we have a gain map.
            return true;
        }
    }

    return false;
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
    if (m_imageInfo.isHeif == true &&
        m_imageInfo.forceBT2100ColorSpace == true)
    {
        CreateHeifHdr10GpuResources();
    }
    else
    {
        ComPtr<ID2D1ImageSourceFromWic> wicImageSource;
        IFRIMG(context->CreateImageSourceFromWic(m_wicCachedSource.Get(), &wicImageSource));
        IFRIMG(wicImageSource.As(&m_imageSource));
    }

    // Xbox One HDR screenshots and HEIF HDR images use the HDR10/BT.2100 colorspace, but this is not represented
    // in a WIC color context so we must manually set behavior.
    if (m_imageInfo.forceBT2100ColorSpace)
    {
        // TODO: Need consistent rules for using IFRIMG vs. IFT (when are errors exceptional?).
        ComPtr<ID2D1ColorContext1> colorContext1;
        IFT(context->CreateColorContextFromDxgiColorSpace(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, &colorContext1));

        IFT(colorContext1.As(&m_colorContext));
    }
    // If the image contains an embedded color profile, use it.
    else if (m_imageInfo.numProfiles >= 1)
    {
        IFT(context->CreateColorContextFromWicColorContext(
            m_wicColorContext.Get(),
            &m_colorContext));
    }
    // If no other info is available, select a default color profile based on pixel format:
    // floating point == scRGB, others == sRGB.
    else
    {
        IFT(context->CreateColorContext(
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
ID2D1TransformedImageSource* ImageLoader::GetLoadedImage(float zoom, bool applyAppleHdrGainMap)
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

    IFT(m_deviceResources->GetD2DDeviceContext()->CreateTransformedImageSource(
        m_imageSource.Get(),
        &props,
        &source));

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
/// For testing only. Obtains the cached WIC source.
/// </summary>
IWICBitmapSource* ImageLoader::GetWicSourceTest()
{
    return m_wicCachedSource.Get();
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
        IFT(WINCODEC_ERR_WRONGSTATE);
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
/// <param name="info">Requires that pixel format info be populated.</param>
/// <param name="source">For some detection types, IWICBitmapFrameDecode is needed. TODO: Not anymore?</param>
void ImageLoader::PopulateImageInfoACKind(ImageInfo& info, _In_ IWICBitmapSource* source)
{
    UNREFERENCED_PARAMETER(source);

    if (info.bitsPerPixel == 0 ||
        info.bitsPerChannel == 0)
    {
        IFRIMG(WINCODEC_ERR_INVALIDPARAMETER);
    }

    info.imageKind = AdvancedColorKind::StandardDynamicRange;

    // Bit depth > 8bpc or color gamut > sRGB signifies a WCG image.
    // The presence of a color profile is used as an approximation for wide gamut.
    if (info.bitsPerChannel > 8 || info.numProfiles >= 1)
    {
        info.imageKind = AdvancedColorKind::WideColorGamut;
    }

    // Currently, all supported floating point images are considered HDR.
    // This includes JPEG XR, OpenEXR, and Radiance RGBE.
    if (info.isFloat == true)
    {
        info.imageKind = AdvancedColorKind::HighDynamicRange;
    }

    // All images using the HDR10/BT.2100 colorspace are HDR. Currently, WIC color contexts cannot
    // represent BT.2100, so all supported BT.2100 images have the force flag set.
    // This includes Xbox One JPEG XR screenshots and HEIF HDR images.
    if (m_imageInfo.forceBT2100ColorSpace == true)
    {
        m_imageInfo.imageKind = AdvancedColorKind::HighDynamicRange;
    }
}

/// <summary>
/// Fills in the bit depth (channel/pixel) and float fields.
/// </summary>
void ImageLoader::PopulatePixelFormatInfo(ImageInfo& info, WICPixelFormatGUID format)
{
    // This format doesn't support IWICComponentInfo, rely on hardcoded knowledge.
    if (format == GUID_WICPixelFormat32bppR10G10B10A2HDR10)
    {
        info.bitsPerChannel = 10;
        info.bitsPerPixel = 32;
        info.isFloat = false;
    }
    else
    {
        auto wicFactory = m_deviceResources->GetWicImagingFactory();
        ComPtr<IWICComponentInfo> componentInfo;
        IFRIMG(wicFactory->CreateComponentInfo(format, &componentInfo));

        ComPtr<IWICPixelFormatInfo2> pixelFormatInfo;
        IFRIMG(componentInfo.As(&pixelFormatInfo));

        WICPixelFormatNumericRepresentation formatNumber;
        IFRIMG(pixelFormatInfo->GetNumericRepresentation(&formatNumber));

        IFRIMG(pixelFormatInfo->GetBitsPerPixel(&info.bitsPerPixel));

        // Calculate the bits per channel (bit depth) using GetChannelMask.
        // This accounts for nonstandard color channel packing and padding, e.g. 32bppRGB,
        // but assumes each channel has equal bits (e.g. RGB565 doesn't work).
        unsigned char channelMaskBytes[sc_MaxBytesPerPixel];
        ZeroMemory(channelMaskBytes, ARRAYSIZE(channelMaskBytes));
        unsigned int maskSize;

        IFRIMG(pixelFormatInfo->GetChannelMask(
            0,  // Read the first color channel.
            ARRAYSIZE(channelMaskBytes),
            channelMaskBytes,
            &maskSize));

        // Count up the number of bits set in the mask for the first color channel.
        for (unsigned int i = 0; i < maskSize * 8; i++)
        {
            unsigned int byte = i / 8;
            unsigned int bit = i % 8;
            if ((channelMaskBytes[byte] & (1 << bit)) != 0)
            {
                info.bitsPerChannel += 1;
            }
        }

        info.isFloat = (WICPixelFormatNumericRepresentationFloat == formatNumber) ? true : false;
    }
}

/// <summary>
/// Detects if the image is an Xbox console HDR screenshot.
/// </summary>
/// <remarks>
/// Xbox console HDR screenshots use JPEG XR with 10-bit precision and a specially
/// crafted ICC profile and/or EXIF color space to designate that they use BT.2100 PQ.
/// Relies on caller to ensure the container is JPEG XR (IWICBitmapDecoder).
/// </remarks>
bool ImageLoader::IsImageXboxHdrScreenshot(IWICBitmapFrameDecode* frame)
{
    WICPixelFormatGUID fmt = {};
    IFT(frame->GetPixelFormat(&fmt));
    if (fmt != GUID_WICPixelFormat32bppBGR101010)
    {
        return false;
    }

    ComPtr<IWICColorContext> color;
    m_deviceResources->GetWicImagingFactory()->CreateColorContext(&color);

    unsigned int actual = 0;
    IFT(frame->GetColorContexts(1, color.GetAddressOf(), &actual));
    if (actual != 1)
    {
        return false;
    }

    WICColorContextType type = WICColorContextType::WICColorContextUninitialized;
    IFT(color->GetType(&type));

    if (type == WICColorContextType::WICColorContextExifColorSpace)
    {
        unsigned int exif = 0;
        IFT(color->GetExifColorSpace(&exif));
        return (exif == 2084); // This is not a standard EXIF color space.
    }
    else if (type == WICColorContextType::WICColorContextProfile)
    {
        // Compare the profile size and header bytes instead of a full binary or functional check.
        unsigned int profSize = 0;
        IFT(color->GetProfileBytes(0, nullptr, &profSize));
        if (profSize != m_xboxHdrIccSize)
        {
            return false;
        }

        unsigned int ignored = 0;
        std::vector<byte> profBytes;
        profBytes.resize(profSize);
        IFT(color->GetProfileBytes(static_cast<UINT>(profBytes.size()), profBytes.data(), &ignored));

        return (0 == memcmp(m_xboxHdrIccHeaderBytes, profBytes.data(), ARRAYSIZE(m_xboxHdrIccHeaderBytes)));
    }
    else
    {
        return false;
    }
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

/// <summary>
/// Some WIC codecs, (HEIF/HEVC, HEIF/AV1, WebP, etc) aren't always present in the OS
/// even though they can be enumerated and created - these are typically loaded from the Store.
/// Attempt to decode a single pixel to ensure the codec is installed.
/// </summary>
/// <returns>
/// Whether the codec was available and decode succeeded.
/// </returns>
bool ImageLoader::CheckCanDecode(_In_ IWICBitmapFrameDecode* frame)
{
    auto fact = m_deviceResources->GetWicImagingFactory();
    ComPtr<IWICBitmap> bitmap;
    
    if (FAILED(fact->CreateBitmapFromSourceRect(frame, 0, 0, 1, 1, &bitmap)))
    {
        return false;
    }
    else
    {
        return true;
    }
}
