//*********************************************************
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

#include "pch.h"

namespace HDRImageViewer
{
    /// <summary>
    /// Supported render effects which are inserted into the render pipeline.
    /// Includes HDR tonemappers and useful visual tools.
    /// Each render effect is implemented as a custom Direct2D effect.
    /// </summary>
    public enum class RenderEffectKind
    {
        HdrTonemap,
        None,
        SdrOverlay,
        LuminanceHeatmap,
        SphereMap
    };

    /// <summary>
    /// Associates a effect type used by the renderer to a descriptive string bound to UI.
    /// </summary>
    public ref class EffectOption sealed
    {
    public:
        EffectOption(Platform::String^ description, RenderEffectKind kind)
        {
            this->description = description;
            this->kind = kind;
        }

        property Platform::String^ Description
        {
            Platform::String^ get() { return description; }
        }

        property RenderEffectKind Kind
        {
            RenderEffectKind get() { return kind; }
        }

    private:
        Platform::String^       description;
        RenderEffectKind        kind;
    };

     /// <summary>
    /// Allows databinding of render options to the UI: image colorspace and render effect/tonemapper.
    /// </summary>
    public ref class RenderOptionsViewModel sealed
    {
    public:
        RenderOptionsViewModel()
        {
            // The first index is chosen by default. Ensure this is in sync with DirectXPage.
            renderEffects = ref new Platform::Collections::VectorView<EffectOption^>
            {
                ref new EffectOption(L"No effect", RenderEffectKind::None),
                ref new EffectOption(L"HDR tonemap", RenderEffectKind::HdrTonemap),
                ref new EffectOption(L"Draw SDR as grayscale", RenderEffectKind::SdrOverlay),
                ref new EffectOption(L"Luminance heatmap", RenderEffectKind::LuminanceHeatmap),
                // TODO: Temporarily disable sphere map in UI for the upcoming app release.
                //ref new EffectOption(L"Draw as spheremap", RenderEffectKind::SphereMap)
            };
        }

        property Windows::Foundation::Collections::IVectorView<EffectOption^>^ RenderEffects
        {
            Windows::Foundation::Collections::IVectorView<EffectOption^>^ get()
            {
                return this->renderEffects;
            }
        }

    private:
        Platform::Collections::VectorView<EffectOption^>^ renderEffects;
    };
}