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
#define D2D_INPUT0_COMPLEX          // The first input is sampled in a complex manner: to calculate the output of a pixel,
                                    // the shader samples more than just the corresponding input coordinate.
#define D2D_REQUIRES_SCENE_POSITION // The shader requires scene position information.

// Note that the custom build step must provide the correct path to find d2d1effecthelpers.hlsli when calling fxc.exe.
#include "d2d1effecthelpers.hlsli"

cbuffer constants : register(b0)
{
    float2 center;      // Offset for cursor pointer.
    float2 sceneSize;   // Size in pixels of the image.
    float zoom;         // Zoom factor. Default = 0.5.
    float dpi;
};

// Adapted from: Sphere mapping by nimitz (twitter: @stormoid)
// https://www.shadertoy.com/view/4sjXW1

#define M_PI  3.14159265
#define M_TAU 3.14159265 * 2.0
#define M_PANSPEED 5.0    // How quickly panning changes the view.
#define M_VIEWSCALE 300   // How many pixels to normalize sample coordinates by.

// Matrix rotation by angle a.
float2x2 rotmat(float a)
{
    float c = cos(a);
    float s = sin(a);
    return float2x2(c, -s, s, c);
}

// Spherical projection, very heavy deformation on the poles.
float4 sphproj(float3 p)
{
    // Unclear how OpenGL doesn't need this offset to stay within [0, 1].
    // Also how M_TAU worked for the divisor.
    float sph_x = atan(p.z / p.x) / M_PI + 0.5;
    float sph_y = acos(-p.y / length(p)) / M_PI;
    //float2 sph = float2(atan(p.z / p.x) / M_TAU, acos(-p.y / length(p)) / M_PI);
    float2 sph = float2(sph_x, sph_y);

    return D2DSampleInput(0, sph);
}

// Translates 3D pointing vector to texture UV coordinates using spherical projection.
float2 SphericalProjection(float3 p)
{
    // Unclear how OpenGL doesn't need this offset to stay within [0, 1].
    // Also how M_TAU worked for the divisor.
    float x = atan(p.z / p.x) / M_PI + 0.5;
    float y = acos(-p.y / length(p)) / M_PI;
    return float2(x, y);
}

D2D_PS_ENTRY(main)
{
    // Map scene coordinates into [-0.5, 0.5]
    float2 coord2D = D2DGetScenePosition().xy;
    coord2D /= sceneSize;
    coord2D -= 0.5;

    //// Test
    //coord2D += 0.5;
    //coord2D *= sceneSize;
    //return D2DSampleInputAtPosition(0, coord2D);

    //coord2D.x *= sceneSize.x / sceneSize.y;
    // test - hardcode aspect ratio
    //coord2D.x *= 16 / 9;

    // Project the spheremap onto a render target at Z distance M_ZOOM.
    float3 coord3D = normalize(float3(coord2D, -zoom));

    // Pi/2 offset to center the image around the front?
    float2x2 mx = rotmat(center.x * M_PANSPEED + M_PI / 2.0);
    float2x2 my = rotmat(center.y * M_PANSPEED);

    coord3D.yz = mul(coord3D.yz, my);
    coord3D.xz = mul(coord3D.xz, mx);

    float2 sampleCoord = SphericalProjection(coord3D);

    // Convert [0, 1] coordinates back to scene pixels
    sampleCoord *= sceneSize;

    return D2DSampleInputAtPosition(0, sampleCoord);
    
    //float4 color = D2DGetInput(0);
    ////float2 toPixel = D2DGetScenePosition().xy - center;

    //// Resample the image based on the new coordinates.
    ////float4 color = D2DSampleInputAtOffset(0, toPixel);

    //float2 viewPos = D2DGetScenePosition().xy - center;

    //float2x2 test = rotmat(5.0f);
    //color.r = test._m00;

    //// Debug: invert color
    ////color.rgb = 1 - color.rgb;

    //return color;
}