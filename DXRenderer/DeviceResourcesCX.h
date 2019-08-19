#pragma once

#include "Common\DeviceResources.h"
#include "HDRImageViewerRenderer.h"

namespace HDRImageViewer
{
    // Helper class that wraps DeviceResources and HDRImageViewerRenderer
    ref class DeviceAndRenderer sealed
    {
    public:
        DeviceAndRenderer(Windows::UI::Xaml::Controls::SwapChainPanel^ panel)
        {
            m_dev = std::make_shared<DX::DeviceResources>();
            m_dev->SetSwapChainPanel(panel);
            m_render = ref new HDRImageViewerRenderer(m_dev);
        }

        void SetSwapChainPanel(Windows::UI::Xaml::Controls::SwapChainPanel^ panel) { m_dev->SetSwapChainPanel(panel); }
        void SetLogicalSize(Windows::Foundation::Size logicalSize) { m_dev->SetLogicalSize(logicalSize); }
        void SetCurrentOrientation(Windows::Graphics::Display::DisplayOrientations currentOrientation) { m_dev->SetCurrentOrientation(currentOrientation); }
        void SetDpi(float dpi) { m_dev->SetDpi(dpi); }
        void SetCompositionScale(float compositionScaleX, float compositionScaleY) { m_dev->SetCompositionScale(compositionScaleX, compositionScaleY); }
        void ValidateDevice() { m_dev->ValidateDevice(); }
        void HandleDeviceLost() { m_dev->HandleDeviceLost(); }
        void RegisterDeviceNotify(DX::IDeviceNotify^ deviceNotify) { m_dev->RegisterDeviceNotify(deviceNotify); }
        void Trim() { m_dev->Trim(); }
        void Present() { m_dev->Present(); }

        property HDRImageViewer::HDRImageViewerRenderer^ Renderer
        {
            HDRImageViewer::HDRImageViewerRenderer^ get() { return m_render; }
        }

    private:
        std::shared_ptr<DX::DeviceResources> m_dev;
        HDRImageViewer::HDRImageViewerRenderer^ m_render;
    };
}