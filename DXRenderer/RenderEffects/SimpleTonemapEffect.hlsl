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

// Custom effects using pixel shaders should use HLSL helper functions defined in
// d2d1effecthelpers.hlsli to make use of effect shader linking.
#define D2D_INPUT_COUNT 1           // The pixel shader takes 1 input texture.
#define D2D_INPUT0_SIMPLE

// Note that the custom build step must provide the correct path to find d2d1effecthelpers.hlsli when calling fxc.exe.
#include "d2d1effecthelpers.hlsli"

cbuffer constants : register(b0)
{
    float inputMax;  // In scRGB values.
    float outputMax; // In scRGB values.
};

float reinhard(float input)
{
    float output = input / inputMax;

    // Vanilla Reinhard normalizes color values to [0, 1].
    // This modification scales to the luminance range of the display.
    output = (output / 1 + output);

    return output * outputMax;
}

// Implements a rudimentary HDR tonemapper using a modified Reinhard operator.

// Like the Direct2D HDR tonemapper, this effect outputs values in scRGB scene-referred
// luminance space, i.e. the output numeric range will exceed [0, 1].
// Therefore, when outputting to an SDR or WCG display, the app must perform
// white level adjustment to bring the numeric range of the output to [0, 1].
D2D_PS_ENTRY(main)
{
    float4 color = D2DGetInput(0);

    color.r = reinhard(color.r);
    color.g = reinhard(color.g);
    color.b = reinhard(color.b);

    return color;
}