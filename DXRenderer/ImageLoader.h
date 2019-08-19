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
// independently register for DX::IDeviceNotify.
//
// Throws WINCODEC_ERR_[foo] HRESULTs in exceptions as these
// match well with the intended error states.
//
//*********************************************************

#pragma once
#include "Common\DeviceResources.h"
#include "ImageInfo.h"

#include <cstdarg>

namespace HDRImageViewer
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

    class ImageLoader
    {
    public:
        ImageLoader(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        ~ImageLoader();

        ImageLoaderState GetState() const { return m_state; };

        ImageInfo LoadImageFromWic(_In_ IStream* imageStream);
        ImageInfo LoadImageFromDirectXTex(_In_ Platform::String^ filename, _In_ Platform::String^ extension);

        ID2D1TransformedImageSource* GetLoadedImage(float zoom);
        ID2D1ColorContext* GetImageColorContext();
        ImageInfo GetImageInfo();

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

            throw ref new Platform::COMException(WINCODEC_ERR_WRONGSTATE);
        }

        void LoadImageCommon(_In_ IWICBitmapSource* source);
        void CreateDeviceDependentResourcesInternal();
        void PopulateImageInfoACKind(_Inout_ ImageInfo* info, _In_ IWICBitmapSource* source);
        bool IsImageXboxHdrScreenshot(_In_ IWICBitmapSource* source);
        GUID TranslateDxgiFormatToWic(DXGI_FORMAT fmt);

        std::shared_ptr<DX::DeviceResources>                    m_deviceResources;

        // Device-independent
        Microsoft::WRL::ComPtr<IWICFormatConverter>             m_formatConvert;
        Microsoft::WRL::ComPtr<IWICColorContext>                m_wicColorContext;

        ImageLoaderState                                        m_state;
        ImageInfo                                               m_imageInfo;

        // Device-dependent
        Microsoft::WRL::ComPtr<ID2D1ImageSourceFromWic>         m_imageSource;
        Microsoft::WRL::ComPtr<ID2D1ColorContext>               m_colorContext;
    };
}
