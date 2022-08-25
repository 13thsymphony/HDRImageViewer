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

// Not a tonemapper, instead demonstrates visually what would happen in SDR mode:
// - Truncates values to 8 bits per channel
// - Converts out of gamut colors to grayscale

// {70937517-E2A1-4436-8DD1-A6BEC57CED62}
DEFINE_GUID(GUID_MaxLuminancePixelShader,
    0x70937517, 0xe2a1, 0x4436, 0x8d, 0xd1, 0xa6, 0xbe, 0xc5, 0x7c, 0xed, 0x62);
// {0DEBA444-958E-4C78-A5C9-4287FB44A550}
DEFINE_GUID(CLSID_CustomMaxLuminanceEffect,
    0xdeba444, 0x958e, 0x4c78, 0xa5, 0xc9, 0x42, 0x87, 0xfb, 0x44, 0xa5, 0x50);


// Our effect contains one transform, which is simply a wrapper around a pixel shader. As such,
// we can simply make the effect itself act as the transform.
class MaxLuminanceEffect : public ID2D1EffectImpl, public ID2D1DrawTransform
{
public:
    // Declare effect registration methods.
    static HRESULT Register(_In_ ID2D1Factory1* pFactory);

    static HRESULT __stdcall CreateMaxLuminanceImpl(_Outptr_ IUnknown** ppEffectImpl);

    // Declare property getter/setters
    HRESULT SetMaxLum(float nits);
    float GetMaxLum() const;

    // Declare ID2D1EffectImpl implementation methods.
    IFACEMETHODIMP Initialize(
        _In_ ID2D1EffectContext* pContextInternal,
        _In_ ID2D1TransformGraph* pTransformGraph
        );

    IFACEMETHODIMP PrepareForRender(D2D1_CHANGE_TYPE changeType);

    IFACEMETHODIMP SetGraph(_In_ ID2D1TransformGraph* pGraph);

    // Declare ID2D1DrawTransform implementation methods.
    IFACEMETHODIMP SetDrawInfo(_In_ ID2D1DrawInfo* pRenderInfo);

    // Declare ID2D1Transform implementation methods.
    IFACEMETHODIMP MapOutputRectToInputRects(
        _In_ const D2D1_RECT_L* pOutputRect,
        _Out_writes_(inputRectCount) D2D1_RECT_L* pInputRects,
        UINT32 inputRectCount
        ) const;

    IFACEMETHODIMP MapInputRectsToOutputRect(
        _In_reads_(inputRectCount) CONST D2D1_RECT_L* pInputRects,
        _In_reads_(inputRectCount) CONST D2D1_RECT_L* pInputOpaqueSubRects,
        UINT32 inputRectCount,
        _Out_ D2D1_RECT_L* pOutputRect,
        _Out_ D2D1_RECT_L* pOutputOpaqueSubRect
        );

    IFACEMETHODIMP MapInvalidRect(
        UINT32 inputIndex,
        D2D1_RECT_L invalidInputRect,
        _Out_ D2D1_RECT_L* pInvalidOutputRect
        ) const;

    // Declare ID2D1TransformNode implementation methods.
    IFACEMETHODIMP_(UINT32) GetInputCount() const;

    // Declare IUnknown implementation methods.
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();
    IFACEMETHODIMP QueryInterface(_In_ REFIID riid, _Outptr_ void** ppOutput);

    // Declare property getter/setter methods.

private:
    MaxLuminanceEffect();
    HRESULT UpdateConstants();

    // This struct defines the constant buffer of our pixel shader.
    struct
    {
        float dpi;
        float maxLuminance;
    } m_constants;

    Microsoft::WRL::ComPtr<ID2D1DrawInfo>      m_drawInfo;
    Microsoft::WRL::ComPtr<ID2D1EffectContext> m_effectContext;
    LONG                                       m_refCount;
    D2D1_RECT_L                                m_inputRect;
    float                                      m_dpi;
};
