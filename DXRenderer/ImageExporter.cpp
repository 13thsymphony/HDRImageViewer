#include "pch.h"
#include "Common\DirectXHelper.h"
#include "ImageExporter.h"
#include "MagicConstants.h"
#include "RenderEffects\SimpleTonemapEffect.h"
#include "DirectXTex.h"

using namespace Microsoft::WRL;

using namespace DXRenderer;

ImageExporter::ImageExporter()
{
    throw ref new Platform::NotImplementedException;
}

ImageExporter::~ImageExporter()
{
}

/// <summary>
/// Converts an HDR image to SDR using a pipeline equivalent to
/// RenderEffectKind::HdrTonemap. Not yet suitable for general purpose use.
/// </summary>
/// <param name="wicFormat">WIC container format GUID (GUID_ContainerFormat...)</param>
void ImageExporter::ExportToSdr(ImageLoader* loader, DeviceResources* res, IStream* stream, GUID wicFormat)
{
    auto ctx = res->GetD2DDeviceContext();

    // Effect graph: ImageSource > ColorManagement  > HDRTonemap > WhiteScale
    // This graph is derived from, but not identical to RenderEffectKind::HdrTonemap.
    // TODO: Is there any way to keep this better in sync with the main render pipeline?
    // Purposely don't try to do anything special with Apple HDR gainmaps because the main image already is SDR.

    ComPtr<ID2D1TransformedImageSource> source = loader->GetLoadedImage(1.0f, false);

    ComPtr<ID2D1Effect> colorManage;
    IFT(ctx->CreateEffect(CLSID_D2D1ColorManagement, &colorManage));
    colorManage->SetInput(0, source.Get());
    IFT(colorManage->SetValue(D2D1_COLORMANAGEMENT_PROP_QUALITY, D2D1_COLORMANAGEMENT_QUALITY_BEST));

    ComPtr<ID2D1ColorContext> sourceCtx = loader->GetImageColorContext();
    IFT(colorManage->SetValue(D2D1_COLORMANAGEMENT_PROP_SOURCE_COLOR_CONTEXT, sourceCtx.Get()));

    ComPtr<ID2D1ColorContext1> destCtx;
    // scRGB
    IFT(ctx->CreateColorContextFromDxgiColorSpace(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709, &destCtx));
    IFT(colorManage->SetValue(D2D1_COLORMANAGEMENT_PROP_DESTINATION_COLOR_CONTEXT, destCtx.Get()));

    ComPtr<ID2D1Effect> tonemap;
    IFT(ctx->CreateEffect(CLSID_D2D1HdrToneMap, &tonemap));
    tonemap->SetInputEffect(0, colorManage.Get());
    IFT(tonemap->SetValue(D2D1_HDRTONEMAP_PROP_OUTPUT_MAX_LUMINANCE, sc_DefaultSdrDispMaxNits));
    IFT(tonemap->SetValue(D2D1_HDRTONEMAP_PROP_DISPLAY_MODE, D2D1_HDRTONEMAP_DISPLAY_MODE_SDR));

    ComPtr<ID2D1Effect> whiteScale;
    IFT(ctx->CreateEffect(CLSID_D2D1ColorMatrix, &whiteScale));
    whiteScale->SetInputEffect(0, tonemap.Get());

    float scale = D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL / sc_DefaultSdrDispMaxNits;
    D2D1_MATRIX_5X4_F matrix = D2D1::Matrix5x4F(
        scale, 0, 0, 0,  // [R] Multiply each color channel
        0, scale, 0, 0,  // [G] by the scale factor in 
        0, 0, scale, 0,  // [B] linear gamma space.
        0, 0, 0, 1,      // [A] Preserve alpha values.
        0, 0, 0, 0);     //     No offset.

    IFT(whiteScale->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, matrix));

    ComPtr<ID2D1Image> d2dImage;
    whiteScale->GetOutput(&d2dImage);

    ImageExporter::ExportToWic(d2dImage.Get(), loader->GetImageInfo().pixelSize, res, stream, wicFormat);
}

/// <summary>
/// Saves a WIC bitmap to DDS image file. Primarily for debug/test purposes, specifically HDR10 HEIF images.
/// </summary>
void ImageExporter::ExportToDds(IWICBitmap* bitmap, IStream* stream, DXGI_FORMAT outputFmt)
{
    ComPtr<IWICBitmapLock> lock;
    IFT(bitmap->Lock({}, WICBitmapLockRead, &lock));
    UINT width, height, stride, size = 0;
    WICInProcPointer data;
    IFT(lock->GetDataPointer(&size, &data));
    IFT(lock->GetSize(&width, &height));
    IFT(lock->GetStride(&stride));

    DirectX::Image img;
    img.format = outputFmt;
    img.width = width;
    img.height = height;
    img.rowPitch = stride;
    img.slicePitch = size;
    img.pixels = data;

    DirectX::Blob blob;

    IFT(DirectX::SaveToDDSMemory(img, DirectX::DDS_FLAGS_NONE, blob));

    ULONG written = 0;
    IFT(stream->Write(blob.GetBufferPointer(), (ULONG)blob.GetBufferSize(), &written));
    IFT(written == blob.GetBufferSize() ? S_OK : E_FAIL);
}

/// <summary>
/// Encodes a buffer of pixels to a PNG in the target stream.
/// </summary>
/// <param name="buffer">Caller is responsible for ownership of memory.</param>
/// <param name="stride"></param>
/// <param name="countBytes"></param>
/// <param name="fmt"></param>
/// <param name="stream"></param>
void ImageExporter::ExportPixels(IWICImagingFactory* fact, unsigned int pixelWidth, unsigned int pixelHeight, byte* buffer, unsigned int stride, unsigned int countBytes, WICPixelFormatGUID fmt, IStream* stream)
{
    ComPtr<IWICBitmap> bitmap;
    IFT(fact->CreateBitmapFromMemory(pixelWidth, pixelHeight, fmt, stride, countBytes, buffer, &bitmap));

    ComPtr<IWICBitmapEncoder> encoder;
    IFT(fact->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));

    ComPtr<IWICBitmapFrameEncode> frame;
    IFT(encoder->CreateNewFrame(&frame, nullptr));
    IFT(frame->Initialize(nullptr));
    IFT(frame->WriteSource(bitmap.Get(), nullptr));

    IFT(frame->Commit());
    IFT(encoder->Commit());
    IFT(stream->Commit(STGC_DEFAULT));
}

/// <summary>
/// Copies a Direct2D Image into CPU accessible memory.
/// Converts to 3 channel RGB FP32 values in scRGB colorspace.
/// </summary>
/// <remarks>
/// Performs rendering on the ID2D1DeviceContext.
/// </remarks>
std::vector<float> ImageExporter::DumpImageToRGBFloat(_In_ DeviceResources* res, _In_ ID2D1Image* image, D2D1_SIZE_U size)
{
    auto ctx = res->GetD2DDeviceContext();

    // Render to intermediate bitmap.
    D2D1_BITMAP_PROPERTIES1 intermediateProps = {};
    intermediateProps.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED);
    intermediateProps.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_TARGET;

    ComPtr<ID2D1Bitmap1> intermediate;
    IFT(ctx->CreateBitmap(size, nullptr, 0, &intermediateProps, &intermediate));

    ComPtr<ID2D1Image> origTarget;
    ctx->GetTarget(&origTarget);
    ctx->SetTarget(intermediate.Get());

    ctx->BeginDraw();

    ctx->DrawImage(image);

    // We ignore D2DERR_RECREATE_TARGET here. This error indicates that the device
    // is lost. It will be handled during the next call to Present.
    HRESULT hr = ctx->EndDraw();
    if (hr != D2DERR_RECREATE_TARGET)
    {
        IFT(hr);
    }

    ctx->SetTarget(origTarget.Get());

    // Create staging surface.
    D2D1_BITMAP_PROPERTIES1 props = {};
    props.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED);
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ;

    ComPtr<ID2D1Bitmap1> staging;
    IFT(ctx->CreateBitmap(size, nullptr, 0, &props, &staging));

    auto rect = D2D1::RectU(0, 0, size.width, size.height);
    IFT(staging->CopyFromBitmap(&D2D1::Point2U(), intermediate.Get(), &rect));

    D2D1_MAPPED_RECT mapped = {};
    IFT(staging->Map(D2D1_MAP_OPTIONS_READ, &mapped));
    unsigned int mappedSize = mapped.pitch * size.height;

    // Note WIC assumes straight alpha, this currently doesn't correctly handle premultiplied alpha.
    auto wic = res->GetWicImagingFactory();
    ComPtr<IWICBitmap> wicBitmap;
    IFT(wic->CreateBitmapFromMemory(size.width, size.height, GUID_WICPixelFormat64bppRGBAHalf, mapped.pitch, mappedSize, mapped.bits, &wicBitmap));

    // 3 channel FP32.
    GUID outFmt = GUID_WICPixelFormat96bppRGBFloat;
    ComPtr<IWICFormatConverter> convert;
    IFT(wic->CreateFormatConverter(&convert));
    IFT(convert->Initialize(wicBitmap.Get(), outFmt, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom));

    ComPtr<IWICComponentInfo> compInfo;
    IFT(wic->CreateComponentInfo(outFmt, &compInfo));

    ComPtr<IWICPixelFormatInfo2> pixelInfo;
    IFT(compInfo.As(&pixelInfo));

    unsigned int bitsPerPixel = 0;
    IFT(pixelInfo->GetBitsPerPixel(&bitsPerPixel));

    unsigned int channelsPerPixel = 0;
    IFT(pixelInfo->GetChannelCount(&channelsPerPixel));

    std::vector<float> pixels = std::vector<float>(size.width * size.height * channelsPerPixel);
    IFT(convert->CopyPixels(
        nullptr,                                                // Rect
        size.width * bitsPerPixel / 8,                          // Stride (bytes)
        static_cast<uint32_t>(pixels.size() * sizeof(float)),   // Total size (bytes)
        reinterpret_cast<byte*>(pixels.data())));              // Buffer

    IFT(staging->Unmap());

    return pixels;
}

/// <summary>
/// Encodes to WIC using default encode options.
/// </summary>
/// <remarks>
/// First converts to FP16 in D2D, then uses the WIC encoder's internal converter.
/// </remarks>
/// <param name="wicFormat">The WIC container format to encode to.</param>
void ImageExporter::ExportToWic(ID2D1Image* img, Windows::Foundation::Size size, DeviceResources* res, IStream* stream, GUID wicFormat)
{
    auto dev = res->GetD2DDevice();
    auto wic = res->GetWicImagingFactory();

    ComPtr<IWICBitmapEncoder> encoder;
    IFT(wic->CreateEncoder(wicFormat, nullptr, &encoder));
    IFT(encoder->Initialize(stream, WICBitmapEncoderNoCache));

    ComPtr<IWICBitmapFrameEncode> frame;
    IFT(encoder->CreateNewFrame(&frame, nullptr));
    IFT(frame->Initialize(nullptr));

    // Workaround for JPEG-XR which does not support FP16 premultiplied alpha; we just don't support PM alpha correctly for now.
    // Need to investigate explicit alpha format conversion.
    D2D1_ALPHA_MODE alpha = (wicFormat == GUID_ContainerFormatWmp) ? D2D1_ALPHA_MODE_STRAIGHT : D2D1_ALPHA_MODE_PREMULTIPLIED;

    // IWICImageEncoder's internal pixel format conversion from float to uint does not perform gamma correction.
    // For simplicity, rely on the IWICBitmapFrameEncode's format converter which does perform gamma correction.
    WICImageParameters params = {
        D2D1::PixelFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, alpha),
        96.0f,                             // DpiX
        96.0f,                             // DpiY
        0,                                 // OffsetX
        0,                                 // OffsetY
        static_cast<uint32_t>(size.Width), // SizeX
        static_cast<uint32_t>(size.Height) // SizeY
    };

    ComPtr<IWICImageEncoder> imageEncoder;
    IFT(wic->CreateImageEncoder(dev, &imageEncoder));
    IFT(imageEncoder->WriteFrame(img, frame.Get(), &params));
    IFT(frame->Commit());
    IFT(encoder->Commit());
    IFT(stream->Commit(STGC_DEFAULT));
}
