//*********************************************************
//
// ImageExporter
//
// Saves image data in various DirectX formats to disk.
// Used for both app features and for testing.
//
//*********************************************************

#pragma once
#include "Common\DeviceResources.h"
#include "ImageLoader.h"

namespace DXRenderer
{
    /// <summary>
    /// RAII wrapper for VARIANT
    /// </summary>
    class CVariant : public VARIANT {
    public:
        CVariant() { VariantInit(this); }
        ~CVariant() { VariantClear(this); }
    };

    class ImageExporter
    {
    public:
        ImageExporter();
        ~ImageExporter();

        static void ExportToSdr(_In_ ImageLoader* loader, _In_ DeviceResources* res, IStream* stream, GUID wicFormat);

        static void ExportToDds(_In_ IWICBitmap* bitmap, _In_ IStream* stream, DXGI_FORMAT outputFmt);

        static void ExportPixels(_In_ IWICImagingFactory* fact, unsigned int pixelWidth, unsigned int pixelHeight, _In_ byte* buffer, unsigned int stride, unsigned int countBytes, WICPixelFormatGUID fmt, _In_ IStream* stream);

        static std::vector<float> DumpImageToRGBFloat(_In_ DeviceResources* res, _In_ ID2D1Image* image, D2D1_SIZE_U size);

        static void ExportToWic(_In_ ID2D1Image* img, Windows::Foundation::Size size, _In_ DeviceResources* res, _In_ IStream* stream, GUID wicFormat, float quality = -1.0f);
    };
}
