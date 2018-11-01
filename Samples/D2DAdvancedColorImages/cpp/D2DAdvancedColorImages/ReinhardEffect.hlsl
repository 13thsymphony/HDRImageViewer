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

#define DESIRED_SCENE_MIDPOINT_LUM 1.0f // In scRGB values; 80 nits.

// Note that the custom build step must provide the correct path to find d2d1effecthelpers.hlsli when calling fxc.exe.
#include "d2d1effecthelpers.hlsli"

cbuffer constants : register(b0)
{
    float sourceAvgLum; // In scRGB values.
    float targetMaxLum; // In scRGB values.
};

// Implements a modified version of the classic Reinhard HDR tonemapper algorithm.
// Like the 1809 Direct2D HDR tonemapper, this effect outputs values in scRGB scene-referred
// luminance space. Therefore, when outputting to an SDR or WCG display, the app must perform
// white level adjustment to bring the numeric range of the output to [0, 1].
D2D_PS_ENTRY(main)
{
    // 1. Operate on each pixel's RGB channel values directly.
    float4 color = D2DGetInput(0);

    // 2. Scale color values so that the scene average luminance maps to a comfortable value
    // to fit typical PC viewing environments.
    color *= DESIRED_SCENE_MIDPOINT_LUM / sourceAvgLum;

    // 3. Compress color values using a version of Reinhard's operator, then scale to [0, targetMaxLum].
    return (color + color / pow(targetMaxLum, 2)) / (color + 1.0f) * targetMaxLum;
}