//*********************************************************
//
// ImageLoader
//
// Manages loading an image from disk or other stream source
// into Direct2D. Handles all codec operations (WIC),
// detecting image info, and providing a Direct2D ImageSource.
//
// ImageLoader relies on the caller to explicitly inform it
// of device lost/restored events, i.e. it does not
// independently register for IDeviceNotify.
//
// Throws WINCODEC_ERR_[foo] HRESULTs in exceptions as these
// match well with the intended error states.
//
//*********************************************************

#pragma once
#include "Common\DeviceResources.h"
#include "ImageInfo.h"
#include "LibHeifHelpers.h"

#include <cstdarg>

namespace DXRenderer
{
    /// <summary>
    /// State machine.
    /// </summary>
    /// <remarks>
    /// Valid transitions:
    /// NotInitialized      --> LoadingSucceeded || LoadingFailed
    /// LoadingFailed       --> [N/A]
    /// LoadingSucceeded    --> NeedDeviceResources
    /// NeedDeviceResources --> LoadingSucceeded
    /// </remarks>
    enum ImageLoaderState
    {
        NotInitialized,
        LoadingSucceeded,
        LoadingFailed,
        NeedDeviceResources // Device resources must be (re)created but otherwise image data is valid.
    };

    /// <summary>
    /// RAII wrapper for PROPVARIANT
    /// </summary>
    class CPropVariant : public PROPVARIANT {
    public:
        CPropVariant() { PropVariantInit(this); }
        ~CPropVariant() { PropVariantClear(this); }
    };

    public enum class ImageLoaderOptionsType
    {
        NoOverrides,
        ForceBT2100,
        CustomSdrColorSpace
    };

    // Loosely corresponds to D2D1_GAMMA1
    public enum class CustomGamma
    {
        Gamma22,
        Gamma10,
        // Gamma2084 // Not supported since we treat this as SDR anyway.
    };

    // Corresponds to D2D1_SIMPLE_COLOR_PROFILE
    [Windows::Foundation::Metadata::WebHostHidden]
    public value struct CustomSdrColorSpace
    {
        Windows::Foundation::Point red; // xy.
        Windows::Foundation::Point green; // xy.
        Windows::Foundation::Point blue; // xy.
        Windows::Foundation::Point whitePt_XZ; // XZ normalized to Y.
        CustomGamma Gamma;
    };

    [Windows::Foundation::Metadata::WebHostHidden]
    public value struct ImageLoaderOptions
    {
        ImageLoaderOptionsType type;
        CustomSdrColorSpace customColorSpace;
    };

    class ImageLoader
    {
    public:
        ImageLoader(const std::shared_ptr<DeviceResources>& deviceResources, ImageLoaderOptions& options);
        ~ImageLoader();

        ImageLoaderState GetState() const { return m_state; };

        ImageInfo LoadImageFromWic(_In_ IStream* imageStream);
        ImageInfo LoadImageFromDirectXTex(_In_ Platform::String^ filename, _In_ Platform::String^ extension);

        ID2D1TransformedImageSource* GetLoadedImage(float zoom, bool selectAppleHdrGainMap);

        ID2D1ColorContext* GetImageColorContext();
        ImageInfo GetImageInfo();
        IWICBitmapSource* GetWicSourceTest();

        void CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();

    private:
        /// <summary>
        /// Throws if the internal ImageLoaderState does not match one of the valid values.
        /// Pass in one or more ImageLoaderState values.
        /// </summary>
        /// <param name="numStates">How many ImageLoaderState values are valid.</param>
        inline void EnforceStates(int numStates...)
        {
            va_list args;
            va_start(args, numStates);

            for (int i = 0; i < numStates; i++)
            {
                auto s = va_arg(args, ImageLoaderState);
                if (m_state == s) return;
            }

            // TODO: We should not rely on EnforceStates to catch unexpected image loading failed.
            // For now, return a more informative error in this case.
            if (m_state == ImageLoaderState::LoadingFailed)
            {
                throw ref new Platform::COMException(WINCODEC_ERR_BADIMAGE);
            }

            throw ref new Platform::COMException(WINCODEC_ERR_WRONGSTATE);
        }

        /// <summary>
        /// "If failed return [ImageLoader variant]"
        /// Only use when errors from malformed files are not exceptional, and we want to
        /// inform the caller this failed.
        /// </summary>
#define IFRIMG(hr) if (FAILED(hr)) { \
                m_imageInfo.isValid = false; \
                m_state = ImageLoaderState::LoadingFailed; \
                return; }

        /// <summary>
        /// "If failed return false"
        /// </summary>
#define IFRF(hr) if (FAILED(hr)) { return false; }

        inline HRESULT HEIFHR(heif_error herr) { return herr.code == heif_error_code::heif_error_Ok ? S_OK : WINCODEC_ERR_GENERIC_ERROR; }

        void LoadImageFromWicInt(_In_ IStream* imageStream);
        void LoadImageFromDirectXTexInt(_In_ Platform::String^ filename, _In_ Platform::String^ extension);
        void LoadImageCommon(_In_ IWICBitmapSource* source);
        void CreateDeviceDependentResourcesInternal();

        void PopulateImageInfoACKind(ImageInfo& info, _In_ IWICBitmapSource* source);
        void PopulatePixelFormatInfo(ImageInfo& info, WICPixelFormatGUID format);
        bool IsImageXboxHdrScreenshot(_In_ IWICBitmapFrameDecode * frame);
        GUID TranslateDxgiFormatToWic(DXGI_FORMAT fmt);
        bool CheckCanDecode(_In_ IWICBitmapFrameDecode* frame);
        void CreateHeifHdr10CpuResources(_In_ IWICBitmapSource* source);
        void CreateHeifHdr10GpuResources();
        bool TryLoadAppleHdrGainMap(_In_ IStream* imageStream);

        std::shared_ptr<DeviceResources>                        m_deviceResources;

        // Device-independent
        Microsoft::WRL::ComPtr<IWICBitmapSource>                m_wicCachedSource;
        Microsoft::WRL::ComPtr<IWICColorContext>                m_wicColorContext;
        CHeifImageWithWicSource                                 m_appleHdrGainMap;

        ImageLoaderState                                        m_state;
        ImageInfo                                               m_imageInfo;
        ImageLoaderOptions                                      m_options;
        D2D1_SIMPLE_COLOR_PROFILE                               m_customColorProfile;

        // Device-dependent. Everything here needs to be reset in ReleaseDeviceDependentResources.
        Microsoft::WRL::ComPtr<ID2D1ImageSource>                m_imageSource;
        Microsoft::WRL::ComPtr<ID2D1ImageSource>                m_hdrGainMapSource;
        Microsoft::WRL::ComPtr<ID2D1ColorContext>               m_colorContext;

        // 128 byte ICC profile header for Xbox console HDR screen captures.
        const unsigned char                                     m_xboxHdrIccHeaderBytes[128];
        const unsigned int                                      m_xboxHdrIccSize;
    };
}
