//*********************************************************
//
// ImageExporter
//
// Saves image data in various DirectX formats to disk.
// Used for both app features and for testing.
//
//*********************************************************

#pragma once
#include "DeviceResources.h"
#include "ImageLoader.h"

namespace D2DAdvancedColorImages
{
    class ImageExporter
    {
    public:
        ImageExporter();
        ~ImageExporter();

        static void ExportToSdr(_In_ ImageLoader* loader, _In_ DX::DeviceResources* res, IStream* stream, GUID wicFormat);

    private:
    };
}
