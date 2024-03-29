#pragma once
#include "pch.h"

namespace DXRenderer
{
    /// <summary>
    /// RAII wrapper for heif_context
    /// </summary>
    class CHeifContext {
    public:
        CHeifContext() { ptr = heif_context_alloc(); if (!ptr) throw; }
        ~CHeifContext() { if (ptr) heif_context_free(ptr); }
        heif_context* ptr = nullptr;
    };

    /// <summary>
    /// RAII wrapper for heif_image_handle
    /// </summary>
    class CHeifHandle {
    public:
        ~CHeifHandle() { if (ptr) heif_image_handle_release(ptr); }
        heif_image_handle* ptr = nullptr;
    };

    /// <summary>
    /// RAII wrapper for heif_image
    /// </summary>
    class CHeifImage {
    public:
        ~CHeifImage() { if (ptr) heif_image_release(ptr); }
        heif_image* ptr = nullptr;
    };
    
    /// <summary>
    /// Links the lifetime of a memory-backed IWICBitmap with its underlying CHeifImage.
    /// Member is IWICBitmapSource so we can chain additional transforms after the Bitmap.
    /// </summary>
    class CHeifImageWithWicSource : public CHeifImage {
    public:
        Microsoft::WRL::ComPtr<IWICBitmapSource> wicSource;
    };

    /// <summary>
    /// RAII wrapper for string received from heif_image_handle_get_auxiliary_type
    /// </summary>
    class CHeifAuxType {
    public:
        ~CHeifAuxType() { if (ptr) free((void*)ptr); }
        bool IsAppleHdrGainMap() { return strcmp(ptr, "urn:com:apple:photo:2020:aux:hdrgainmap") == 0; }
        const char* ptr = nullptr;
    };
}