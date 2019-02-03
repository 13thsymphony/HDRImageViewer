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

namespace D2DAdvancedColorImages
{
    // The brightness adjustment slider maps the UI linear value to an exponential (power of 2) multiplier factor.
    // The user sees the multiplier as the tooltip text, and the renderer uses this multiplier internally.
    public ref class SdrBrightnessFormatter sealed : Windows::UI::Xaml::Data::IValueConverter
    {
    public:
        virtual Platform::Object^ Convert(Platform::Object^ value, Windows::UI::Xaml::Interop::TypeName targetType,
            Platform::Object^ parameter, Platform::String^ language)
        {
            auto type = value->GetType()->FullName;

            double dbl = static_cast<double>(value);

            float percent = SdrBrightnessFormatter::SliderToBrightness(dbl) * 100.0f;

            Platform::String^ text = ref new Platform::String(std::to_wstring(static_cast<int>(percent)).c_str()) + L"%";

            return text;
        }

        // No need to implement converting back on a one-way binding 
        virtual Platform::Object^ ConvertBack(Platform::Object^ value, Windows::UI::Xaml::Interop::TypeName targetType,
            Platform::Object^ parameter, Platform::String^ language)
        {
            throw ref new Platform::NotImplementedException();
        }

        // Convert slider UI value (linear) to brightness multiplier (2^x)
        static float SliderToBrightness(double slider)
        {
            return powf(2.0f, static_cast<float>(slider));
        }

        // Convert brightness multiplier (2^x) to slider UI value (linear)
        static double BrightnessToSlider(float multiplier)
        {
            multiplier = max(FLT_MIN, multiplier);

            return logf(multiplier) / logf(2.0f);
        }
    };
}