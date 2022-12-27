// Custom effect that draws any colors that exceed the display's max luminance as black.
// Similar to "gamut warning" effects in HDR-enabled image editors.
// TODO: Eventually should consider all aspects of display gamut.

// Custom effects using pixel shaders should use HLSL helper functions defined in
// d2d1effecthelpers.hlsli to make use of effect shader linking.
#define D2D_INPUT_COUNT 1           // The pixel shader takes exactly 1 input.

// Note that the custom build step must provide the correct path to find d2d1effecthelpers.hlsli when calling fxc.exe.
#include "d2d1effecthelpers.hlsli"

cbuffer constants : register(b0)
{
    float dpi : packoffset(c0.x); // Ignored - there is no position-dependent behavior in the shader.
    float maxLuminance : packoffset(c0.y);
};

D2D_PS_ENTRY(main)
{
    float4 output = D2DGetInput(0);

    // Calculate luminance in nits
    float nits = dot(float3(0.2126f, 0.7152f, 0.0722f), output.rgb) * 80.0f;

    // Detect if luminance is outside the max luminance of display
    float isOutsideMaxLum = step(maxLuminance - nits, nits - maxLuminance); // 1 = out, 0 = in
    float isInsideMaxLum = 1 - isOutsideMaxLum;

    float4 outsideMaxLumColor = float4(0.0f, 0.0f, 0.0f, 1.0f); // outside range is opaque black

#if false
    // Some alternate render behaviors for out-of-gamut colors :
    float4 outsideMaxLumColor = float4(1.0f, 0.0f, 1.0f, 1.0f); // outside range is magenta
    float lum = dot(float3(0.3f, 0.59f, 0.11f), output.rgb);
    float4 outsideMaxLumColor = float4(lum, lum, lum, 1.0f); // outside range is grayscale, this operation isn't very noticable
    float4 outsideMaxLumColor = float4(1.0f, 1.0f, 1.0f, 0.0f) - saturate(output); // outside range is inverted colors
#endif

    float4 insideMaxLumColor = float4(output.rgb, 1.0f);

    return isInsideMaxLum * insideMaxLumColor + isOutsideMaxLum * outsideMaxLumColor;
}