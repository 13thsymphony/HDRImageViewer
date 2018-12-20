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

#define MIDTONE_MAX 0.8   // Fraction of outputMax reserved for passthrough linear section.
#define DELTA 0.00001

// Note that the custom build step must provide the correct path to find d2d1effecthelpers.hlsli when calling fxc.exe.
#include "d2d1effecthelpers.hlsli"

cbuffer constants : register(b0)
{
    float inputMax;  // In scRGB values.
    float outputMax; // In scRGB values.
};

// Rational, second-order, weighted bezier. t is defined over [0, 1].
float rb2(float t, float p0, float p1, float p2, float w0, float w1, float w2)
{
    float tN1 = 1 - t;
    float tN1_2 = tN1 * tN1;
    float t_2 = t * t;

    // w0 * (1 - t)^2 * p0 +
    // w1 * 2t(1 - t) * p1 +
    // w2 * t^2       * p2
    float num = w0 * tN1_2 * p0 + w1 * 2 * t * tN1 * p1 + w2 * t_2 * p2;

    // w0 * (1 - t)^2 +
    // w1 * 2t(1 - t) +
    // w2 * t^2
    float den = w0 * tN1_2 + w1 * 2 * t * tN1 + w2 * t_2;

    return num / den;
}

// This tonemapper operates directly on each R, G and B channel. A more sophisticated tonemapper
// would first convert to a colorspace like ICtCp that isolates luminance.
float channelTonemap(float input)
{
    // Tonemapper consists of 3 segments:
    // 1: Midtones and shadows are passed through/preserved.
    // 2: Highlights that exceed display max luminance are compressed using bezier.
    // 3: Highlights that exceed content max (i.e. bad metadata) are clipped.
    if (input < MIDTONE_MAX * outputMax)
    {
        return input;
    }
    else if (input < inputMax)
    {
        float midLimit = MIDTONE_MAX * outputMax;
        float w0 = outputMax / midLimit;
        float w1 = inputMax / outputMax;
        float t = (input - midLimit) / (inputMax - midLimit + DELTA);

        // Choose weights for smooth transition with linear segments.
        return rb2(
            t,
            midLimit, outputMax, outputMax, // p0, p1, p2
            w0      , w1       , w1);
    }
    else
    {
        return outputMax;
    }
}

// Implements a rudimentary HDR tonemapper. In linear RGB (scRGB) space, preserve colors which
// the display can reproduce, and use a rational bezier to provide a "soft knee" to compress
// colors above the display (target) max luminance.

// Like the 1809 Direct2D HDR tonemapper, this effect outputs values in scRGB scene-referred
// luminance space. Therefore, when outputting to an SDR or WCG display, the app must perform
// white level adjustment to bring the numeric range of the output to [0, 1].
D2D_PS_ENTRY(main)
{
    float4 color = D2DGetInput(0);

    color.r = channelTonemap(color.r);
    color.g = channelTonemap(color.g);
    color.b = channelTonemap(color.b);

    return color;
}