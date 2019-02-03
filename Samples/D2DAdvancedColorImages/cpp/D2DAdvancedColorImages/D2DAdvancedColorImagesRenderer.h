﻿//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include "DeviceResources.h"
#include "SdrOverlayEffect.h"
#include "LuminanceHeatmapEffect.h"
#include "SphereMapEffect.h"
#include "RenderOptions.h"

namespace D2DAdvancedColorImages
{
    struct ImageInfo
    {
        unsigned int                                    bitsPerPixel;
        unsigned int                                    bitsPerChannel;
        bool                                            isFloat;
        Windows::Foundation::Size                       size;
        unsigned int                                    numProfiles;
        Windows::Graphics::Display::AdvancedColorKind   imageKind;
        bool                                            isXboxHdrScreenshot;
        bool                                            isValid;
    };

    struct ImageCLL
    {
        float   maxNits;
        float   medNits;
    };

    class D2DAdvancedColorImagesRenderer : public DX::IDeviceNotify
    {
    public:
        D2DAdvancedColorImagesRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        ~D2DAdvancedColorImagesRenderer();

        void CreateDeviceIndependentResources();
        void CreateDeviceDependentResources();
        void CreateWindowSizeDependentResources();
        void ReleaseDeviceDependentResources();

        void Draw();

        void CreateImageDependentResources();
        void ReleaseImageDependentResources();

        void UpdateManipulationState(_In_ Windows::UI::Input::ManipulationUpdatedEventArgs^ args);

        // Returns the computed MaxCLL and AvgCLL of the image in nits. While HDR metadata is a
        // property of the image (and is independent of rendering), our implementation
        // can't compute it until this point.
        ImageCLL FitImageToWindow(bool computeMetadata);

        void SetRenderOptions(
            RenderEffectKind effect,
            float brightnessAdjustment,
            Windows::Graphics::Display::AdvancedColorInfo^ acInfo
            );

        ImageInfo LoadImageFromWic(_In_ IStream* imageStream);
        ImageInfo LoadImageFromDirectXTex(_In_ Platform::String^ filename, _In_ Platform::String^ extension);

        void PopulateImageInfoACKind(_Inout_ ImageInfo* info);

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

        void LoadImageCommon(_In_ IWICBitmapSource* source);
        void CreateHistogramResources();
        bool IsImageXboxHdrScreenshot(_In_ IWICBitmapSource* source);
        GUID TranslateDxgiFormatToWic(DXGI_FORMAT fmt);

        void UpdateImageColorContext();
        void UpdateWhiteLevelScale(float brightnessAdjustment, float sdrWhiteLevel);
        void UpdateImageTransformState();
        void ComputeHdrMetadata();
        void EmitHdrMetadata();

        float GetBestDispMaxLuminance();

        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources>                    m_deviceResources;

        // WIC and Direct2D resources.
        Microsoft::WRL::ComPtr<IWICFormatConverter>             m_formatConvert;
        Microsoft::WRL::ComPtr<IWICColorContext>                m_wicColorContext;
        Microsoft::WRL::ComPtr<ID2D1ImageSourceFromWic>         m_imageSource;
        Microsoft::WRL::ComPtr<ID2D1TransformedImageSource>     m_scaledImage;
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
    };
}