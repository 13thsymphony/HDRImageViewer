//*********************************************************
//
// HDRImageViewerRenderer
//
// Main manager of all native DirectX image rendering resources.
//
//*********************************************************

#pragma once

#include "Common\DeviceResources.h"
#include "RenderEffects\SdrOverlayEffect.h"
#include "RenderEffects\LuminanceHeatmapEffect.h"
#include "RenderEffects\SphereMapEffect.h"
#include "RenderOptions.h"
#include "ImageLoader.h"

namespace DXRenderer
{
    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class HDRImageViewerRenderer sealed : public DXRenderer::IDeviceNotify
    {
    public:
        HDRImageViewerRenderer(Windows::UI::Xaml::Controls::SwapChainPanel^ panel);

        // DeviceResources wrapper methods for Windows Runtime Component
        void SetSwapChainPanel(Windows::UI::Xaml::Controls::SwapChainPanel^ panel) { m_deviceResources->SetSwapChainPanel(panel); }
        void SetLogicalSize(Windows::Foundation::Size logicalSize)                 { m_deviceResources->SetLogicalSize(logicalSize); }
        void SetCurrentOrientation(Windows::Graphics::Display::DisplayOrientations currentOrientation)
                                                                                   { m_deviceResources->SetCurrentOrientation(currentOrientation); }
        void SetDpi(float dpi)                                                     { m_deviceResources->SetDpi(dpi); }
        void SetCompositionScale(float compositionScaleX, float compositionScaleY) { m_deviceResources->SetCompositionScale(compositionScaleX, compositionScaleY); }
        void ValidateDevice()                                                      { m_deviceResources->ValidateDevice(); }
        void HandleDeviceLost()                                                    { m_deviceResources->HandleDeviceLost(); }
        void Trim()                                                                { m_deviceResources->Trim(); }
        void Present()                                                             { m_deviceResources->Present(); }

        void CreateDeviceIndependentResources();
        void CreateDeviceDependentResources();
        void CreateWindowSizeDependentResources();
        void ReleaseDeviceDependentResources();

        void Draw();

        void CreateImageDependentResources();
        void ReleaseImageDependentResources();
        void SetTargetCpuReadbackSupport(bool value);
        Windows::Foundation::Numerics::float4 GetPixelColorValue(Windows::Foundation::Point point);

        void UpdateManipulationState(_In_ Windows::UI::Input::ManipulationUpdatedEventArgs^ args);

        // Returns the computed MaxCLL and AvgCLL of the image in nits. While HDR metadata is a
        // property of the image (and is independent of rendering), our implementation
        // can't compute it until this point.
        ImageCLL FitImageToWindow(bool computeMetadata);

        void SetRenderOptions(
            RenderEffectKind effect,
            float brightnessAdjustment,
            float dispMaxCllOverride,
            Windows::Graphics::Display::AdvancedColorInfo^ acInfo
            );

        ImageInfo LoadImageFromWic(_In_ Windows::Storage::Streams::IRandomAccessStream^ imageStream, ImageLoaderOptions options);
        ImageInfo LoadImageFromDirectXTex(_In_ Platform::String^ filename, _In_ Platform::String^ extension, ImageLoaderOptions options);
        void      ExportImageToSdr(_In_ Windows::Storage::Streams::IRandomAccessStream^ outputStream, Platform::Guid wicFormat);
        void      ExportAsDdsTest(_In_ Windows::Storage::Streams::IRandomAccessStream^ outputStream);

        // IDeviceNotify methods handle device lost and restored.
        virtual void OnDeviceLost();
        virtual void OnDeviceRestored();

    private:
        inline static float Clamp(float v, float bound1, float bound2)
        {
            float low = min(bound1, bound2);
            float high = max(bound1, bound2);
            return (v < low) ? low : (v > high) ? high : v;
        }

        ~HDRImageViewerRenderer();

        void CreateHistogramResources();
        void UpdateWhiteLevelScale(float brightnessAdjustment, float sdrWhiteLevel);
        void UpdateImageTransformState();
        void ComputeHdrMetadata();
        void EmitHdrMetadata();

        float GetBestDispMaxLuminance();

        // Cached pointer to device resources.
        std::shared_ptr<DeviceResources>                        m_deviceResources;
        std::unique_ptr<ImageLoader>                            m_imageLoader;

        // WIC and Direct2D resources.
        Microsoft::WRL::ComPtr<ID2D1TransformedImageSource>     m_loadedImage;
        Microsoft::WRL::ComPtr<ID2D1Effect>                     m_colorManagementEffect;
        Microsoft::WRL::ComPtr<ID2D1Effect>                     m_whiteScaleEffect;
        Microsoft::WRL::ComPtr<ID2D1Effect>                     m_sdrWhiteScaleEffect;
        Microsoft::WRL::ComPtr<ID2D1Effect>                     m_hdrTonemapEffect;
        Microsoft::WRL::ComPtr<ID2D1Effect>                     m_sdrOverlayEffect;
        Microsoft::WRL::ComPtr<ID2D1Effect>                     m_heatmapEffect;
        Microsoft::WRL::ComPtr<ID2D1Effect>                     m_sphereMapEffect;
        Microsoft::WRL::ComPtr<ID2D1Effect>                     m_histogramPrescale;
        Microsoft::WRL::ComPtr<ID2D1Effect>                     m_histogramEffect;
        Microsoft::WRL::ComPtr<ID2D1Effect>                     m_finalOutput;

        std::vector<float>                                      m_renderTargetCpuPixels;

        // Other renderer members.
        RenderEffectKind                                        m_renderEffectKind;
        float                                                   m_zoom;
        float                                                   m_minZoom;
        D2D1_POINT_2F                                           m_imageOffset;
        D2D1_POINT_2F                                           m_pointerPos;
        ImageCLL                                                m_imageCLL;
        float                                                   m_brightnessAdjust;
        Windows::Graphics::Display::AdvancedColorInfo^          m_dispInfo;
        ImageInfo                                               m_imageInfo;
        bool                                                    m_isComputeSupported;
        float                                                   m_dispMaxCLLOverride;
        bool                                                    m_enableTargetCpuReadback;
    };
}
