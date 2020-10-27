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
    class ImageExporter
    {
    public:
        ImageExporter();
        ~ImageExporter();

        static void ExportToSdr(_In_ ImageLoader* loader, _In_ DeviceResources* res, IStream* stream, GUID wicFormat);

        static void ExportToDds(_In_ IWICBitmap* bitmap, _In_ IStream* stream, DXGI_FORMAT outputFmt);

        static std::vector<float> DumpImageToRGBFloat(_In_ DeviceResources* res, _In_ ID2D1Image* image, D2D1_SIZE_U size);

    private:
        static void ExportToWic(_In_ ID2D1Image* img, Windows::Foundation::Size size, _In_ DeviceResources* res, IStream* stream, GUID wicFormat);
    };
}
