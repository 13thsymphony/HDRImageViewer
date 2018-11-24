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

#define DESIRED_SCENE_MIDPOINT_LUM 0.2f // In scRGB values; 80 nits.

// Note that the custom build step must provide the correct path to find d2d1effecthelpers.hlsli when calling fxc.exe.
#include "d2d1effecthelpers.hlsli"

cbuffer constants : register(b0)
{
    float sourceAvgLum; // In scRGB values.
    float targetMaxLum; // In scRGB values.
};

// Implements a rudimentary HDR tonemapper. In linear RGB (scRGB) space, preserve colors which
// the display can reproduce, and use a rational bezier to provide a "soft knee" to compress
// colors above the display (target) max luminance.

// Like the 1809 Direct2D HDR tonemapper, this effect outputs values in scRGB scene-referred
// luminance space. Therefore, when outputting to an SDR or WCG display, the app must perform
// white level adjustment to bring the numeric range of the output to [0, 1].
D2D_PS_ENTRY(main)
{
    // Placeholder
    float4 color = D2DGetInput(0);

    return color;
}