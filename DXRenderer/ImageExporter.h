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

        static std::vector<DirectX::XMFLOAT4> DumpD2DTarget(_In_ DeviceResources* res);

    private:
        static void ExportToWic(_In_ ID2D1Image* img, Windows::Foundation::Size size, _In_ DeviceResources* res, IStream* stream, GUID wicFormat);
    };
}
